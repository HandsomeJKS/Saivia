//
// Game.cpp
//

#include "pch.h"
#include "Game.h"

extern void ExitGame();

using namespace DirectX;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

namespace
{	
	const XMVECTORF32 START_POSITION = { 0.f, 1.f, -4.f, 0.f };
	const float ROTATION_GAIN = 0.004f;
	const float MOVEMENT_GAIN = 0.07f;
}

static HWND hWnd;

Game::Game() noexcept(false) :
	m_pitch(0),
	m_yaw(0)
{
	m_deviceResources = std::make_unique<DX::DeviceResources>();
	m_deviceResources->RegisterDeviceNotify(this);

	m_cameraPos = START_POSITION.v;

	currentPos = { 0.f, 0.f, 0.f };
	currentT = { 0.f, 0.f, 1.f };
	currentB = { 1.f, 0.f, 0.f };
	currentN = { 0.f, 1.f, 0.f };

	// Identity
	// 1 0 0 0
	// 0 1 0 0 
	// 0 0 1 0
	// => B N T
}

Game::~Game()
{
	if (m_deviceResources)
	{
		m_deviceResources->WaitForGpu();
		// ImGui
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}
}

// Initialize the Direct3D resources required to run.
void Game::Initialize(HWND window, int width, int height)
{
	hWnd = window;

	m_deviceResources->SetWindow(window, width, height);

	m_deviceResources->CreateDeviceResources();
	CreateDeviceDependentResources();

	m_deviceResources->CreateWindowSizeDependentResources();
	CreateWindowSizeDependentResources();

	// ImGui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImGui_ImplWin32_Init(window);
	ImGui_ImplDX12_Init(m_deviceResources->GetD3DDevice(),
		m_deviceResources->GetBackBufferCount(),
		DXGI_FORMAT_R8G8B8A8_UNORM,
		m_deviceResources->GetImGuiSRV()->GetCPUDescriptorHandleForHeapStart(),	// 在DeviceResource.h加入ID3D12DescriptorHeap SRV
		m_deviceResources->GetImGuiSRV()->GetGPUDescriptorHandleForHeapStart());
	ImGui::StyleColorsLight();

	// GraphicsMemory(for Model)
	m_graphicsMemory = std::make_unique<GraphicsMemory>(m_deviceResources->GetD3DDevice());
	
	// Controller
	m_keyboard = std::make_unique<Keyboard>();
	m_mouse = std::make_unique<Mouse>();
	m_mouse->SetWindow(window);


	// ref position geometirc
	RenderTargetState rtState(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D32_FLOAT);

	EffectPipelineStateDescription pd(
		&GeometricPrimitive::VertexType::InputLayout,
		CommonStates::Opaque,
		CommonStates::DepthDefault,
		CommonStates::CullNone,
		rtState);

	m_effect = std::make_unique<BasicEffect>(m_deviceResources->GetD3DDevice(), EffectFlags::Lighting, pd);
	m_effect->EnableDefaultLighting();

	m_shape = GeometricPrimitive::CreateCube(0.5f);	

	// Test Lua Her
	lua_State* ls; //lua狀態機
	ls = luaL_newstate();
	luaopen_base(ls);
	luaL_openlibs(ls);
	luaL_dofile(ls, "test.lua");

	lua_getglobal(ls, "add"); //取得lua中的變量
	lua_pushnumber(ls, (double)30); //將第1個參數壓入lua
	lua_pushnumber(ls, (double)20); //將第2個參數壓入lua
	
	lua_call(ls, 2, 1); //call lua function, 4個參數, 0個回傳數
	int n = lua_tonumber(ls, -1);
	lua_pop(ls, 1); //pop
	lua_close(ls);

	// TODO: Change the timer settings if you want something other than the default variable timestep mode.
	// e.g. for 60 FPS fixed timestep update logic, call:
	/*
	m_timer.SetFixedTimeStep(true);
	m_timer.SetTargetElapsedSeconds(1.0 / 60);
	*/
}

#pragma region Frame Update
// Executes the basic game loop.
void Game::Tick()
{
	m_timer.Tick([&]()
	{
		Update(m_timer);
	});

	Render();
}

// Updates the world.
void Game::Update(DX::StepTimer const& timer)
{
	PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

	float elapsedTime = float(timer.GetElapsedSeconds());

	// TODO: Add your game logic here.
	elapsedTime;

	// Controller
	auto kb = m_keyboard->GetState();
	if (kb.Escape)
	{
		ExitGame();
	}

	if (kb.Home)
	{
		m_cameraPos = START_POSITION.v;
		m_pitch = m_yaw = 0;
	}

	if (kb.F5)
	{
		// Reload Scene

		m_deviceResources->WaitForGpu();
		// ModelList Reset!!
		RailwayDataList.clear();
		RailwayPosList.clear();
		jsonEngine.clear();

		currentPos = { 0.f, 0.f, 0.f };
		currentT = { 0.f, 0.f, 1.f };
		currentB = { 1.f, 0.f, 0.f };
		currentN = { 0.f, 1.f, 0.f };

		std::ifstream jsonData("Assets\\World.json");
		jsonData >> jsonEngine;
		SceneParser();
	}

	if (kb.L)
	{
		isCameraLock = true;
	}

	if (kb.F)
	{
		isCameraLock = false;
	}

	Vector3 move = Vector3::Zero;

	if (kb.Up || kb.W) {
		if (isCameraLock)
		{

		}
		else
		{
			move.z += 1.f;
		}
	}

	if (kb.Down || kb.S) {
		if (isCameraLock)
		{

		}
		else
		{
			move.z -= 1.f;
		}
	}

	if (kb.Left || kb.A)
		move.x += 1.f;

	if (kb.Right || kb.D)
		move.x -= 1.f;

	if (kb.PageUp || kb.Space)
		move.y += 1.f;

	if (kb.PageDown || kb.X)
		move.y -= 1.f;

	Quaternion q = Quaternion::CreateFromYawPitchRoll(m_yaw, m_pitch, 0.f);

	move = Vector3::Transform(move, q);

	move *= MOVEMENT_GAIN;

	m_cameraPos += move;

	auto mouse = m_mouse->GetState();
	if (mouse.positionMode == Mouse::MODE_RELATIVE)
	{
		Vector3 delta = Vector3(float(mouse.x), float(mouse.y), 0.f)
			* ROTATION_GAIN;

		m_pitch -= delta.y;
		m_yaw -= delta.x;

		// limit pitch to straight up or straight down
		// with a little fudge-factor to avoid gimbal lock
		float limit = XM_PI / 2.0f - 0.01f;
		m_pitch = std::max(-limit, m_pitch);
		m_pitch = std::min(+limit, m_pitch);

		// keep longitude in sane range by wrapping
		if (m_yaw > XM_PI)
		{
			m_yaw -= XM_PI * 2.0f;
		}
		else if (m_yaw < -XM_PI)
		{
			m_yaw += XM_PI * 2.0f;
		}
	}

	m_mouse->SetMode(mouse.leftButton ? Mouse::MODE_RELATIVE : Mouse::MODE_ABSOLUTE);

	PIXEndEvent();
}
#pragma endregion

#pragma region Frame Render
// Draws the scene.
void Game::Render()
{
	// Don't try to render anything before the first Update.
	if (m_timer.GetFrameCount() == 0)
	{
		return;
	}

	// Prepare the command list to render a new frame.
	m_deviceResources->Prepare();
	Clear();

	auto commandList = m_deviceResources->GetCommandList();
	PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render");

	// TODO: Add your rendering code here.

	// Camera
	float y = sinf(m_pitch);
	float r = cosf(m_pitch);
	float z = r * cosf(m_yaw);
	float x = r * sinf(m_yaw);

	Vector3 lookAt = m_cameraPos + Vector3(x, y, z);

	m_view = XMMatrixLookAtRH(m_cameraPos, lookAt, Vector3::Up);

	/* Render Cube for ref position*/
	auto cubeWorld = Matrix::Identity;
	m_effect->SetMatrices(cubeWorld, m_view, m_proj);
	m_effect->Apply(commandList);
	m_shape->Draw(commandList);

	// Draw Model
	if (m_model != nullptr)
	{
		ID3D12DescriptorHeap* heaps[] = { m_modelResources->Heap(), m_states->Heap() };
		commandList->SetDescriptorHeaps(_countof(heaps), heaps);
		/*
		m_world = Matrix(currentB, currentN, currentT);
		m_world *= DirectX::SimpleMath::Matrix::CreateTranslation(
			DirectX::SimpleMath::Vector3{ 0.f, 0.f, 0.f });
		Model::UpdateEffectMatrices(m_modelNormal, m_world, m_view, m_proj);
		m_model->Draw(commandList, m_modelNormal.cbegin());	
		*/
		
		for (auto Data_World : RailwayDataList)
		{
			m_world = Matrix::Identity;
			Model::UpdateEffectMatrices(m_modelNormal, Data_World, m_view, m_proj);
			m_model->Draw(commandList, m_modelNormal.cbegin());
		}
		
	}

	// ImGui
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();

	ImGui::NewFrame();
	ImGui::Begin("Main");
	ImGui::Text("FPS: %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
	ImGui::Text("Camera Position: x: %.3f y: %.3f z: %.3f ", m_cameraPos.x, m_cameraPos.y, m_cameraPos.z);
	ImGui::Text("Look At: x: %.3f y: %.3f z: %.3f ", lookAt.x, lookAt.y, lookAt.z);
	ImGui::End();

	if (RWItemUI) {
		ImGui::Begin("Railway Items");
		ImGui::BeginChild("Scrolling");
		for (int n = 0; n < RailwayPosList.size(); n++) {			
			ImGui::Text("Item %d : x: %.3f y: %.3f z: %.3f ", n+1,
				RailwayPosList[n].x,
				RailwayPosList[n].y,
				RailwayPosList[n].z);
		}
		ImGui::EndChild();
		ImGui::End();
	}

	if (ModelUI) {
		ImGui::Begin("Model");
		ImGui::End();
	}
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{			
			if (ImGui::MenuItem("Exit")) { 				
				ExitGame();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Model"))
		{
			if (ImGui::MenuItem("Convert & Import Model")) {
				// ModelUI = true; 
				// Load Model Here!

				std::wstring outputFile;
				std::wstring outputFile_path;

				// Open File Dialog
				// https://docs.microsoft.com/en-us/previous-versions/windows/desktop/legacy/bb776913(v=vs.85)
				IFileDialog *pfd = NULL;
				HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog,
					NULL,
					CLSCTX_INPROC_SERVER,
					IID_PPV_ARGS(&pfd));
				if (SUCCEEDED(hr))
				{
					hr = pfd->Show(NULL);
					if (SUCCEEDED(hr))
					{
						IShellItem *psiResult;
						hr = pfd->GetResult(&psiResult);
						if (SUCCEEDED(hr))
						{
							PWSTR pszFilePath = NULL;
							hr = psiResult->GetDisplayName(SIGDN_FILESYSPATH,
								&pszFilePath);
							if (SUCCEEDED(hr))
							{
								auto file_path = std::filesystem::path(std::wstring(pszFilePath)).remove_filename();
								auto file_name = std::filesystem::path(std::wstring(pszFilePath)).stem();
								outputFile =
									std::wstring(file_path) +
									std::wstring(file_name) +
									L".sdkmesh";

								outputFile_path = std::wstring(file_path);

								std::wstring cmd =
									std::wstring(std::filesystem::current_path()) +
									L"/tool/meshconvert " +
									std::wstring(pszFilePath) +
									L" -sdkmesh -nodds -y -flipv -o " +
									outputFile;

								_wsystem(cmd.c_str());
							}
							psiResult->Release();
						}
					}
				}
				pfd->Release();
				if (outputFile != L"") {
					m_states = std::make_unique<CommonStates>(m_deviceResources->GetD3DDevice());

					m_model = Model::CreateFromSDKMESH(outputFile.c_str());

					ResourceUploadBatch resourceUpload(m_deviceResources->GetD3DDevice());

					resourceUpload.Begin();

					m_model->LoadStaticBuffers(m_deviceResources->GetD3DDevice(), resourceUpload);

					if (!m_model->textureNames.empty())
					{
						for (auto &texName : m_model->textureNames)
						{
							texName = outputFile_path + texName;
						}
						// m_model->textureNames[0] = outputFile_path + m_model->textureNames[0];
						m_modelResources = m_model->LoadTextures(m_deviceResources->GetD3DDevice(), resourceUpload);

						m_fxFactory = std::make_unique<EffectFactory>(m_modelResources->Heap(), m_states->Heap());

						auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());

						uploadResourcesFinished.wait();

						// RenderTargetState rtState(DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_D32_FLOAT);
						RenderTargetState rtState(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D32_FLOAT);

						EffectPipelineStateDescription pd(
							nullptr,
							CommonStates::Opaque,
							CommonStates::DepthDefault,
							CommonStates::CullClockwise,
							rtState);

						EffectPipelineStateDescription pdAlpha(
							nullptr,
							CommonStates::AlphaBlend,
							CommonStates::DepthDefault,
							CommonStates::CullClockwise,
							rtState);

						m_modelNormal = m_model->CreateEffects(*m_fxFactory, pd, pdAlpha);

						m_world = Matrix::Identity;
					}
					else
					{
						m_model.reset();
						MessageBox(hWnd, L"Model NO Texture!!", L"Error", NULL);
					}
				}
			}
			if (ImGui::MenuItem("Import")) {}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Scene")) 
		{
			if (ImGui::MenuItem("Load Scene")) { 
				// 未來考慮用winrt/c++ 的hstring
				std::ifstream jsonData("Assets\\World.json");				
				jsonData >> jsonEngine;
				SceneParser();
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("..."))
		{
			if (ImGui::MenuItem("Undo", "CTRL+Z")) {}
			if (ImGui::MenuItem("Redo", "CTRL+Y", false, false)) {}  // Disabled item
			ImGui::Separator();
			if (ImGui::MenuItem("Cut", "CTRL+X")) {}
			if (ImGui::MenuItem("Copy", "CTRL+C")) {}
			if (ImGui::MenuItem("Paste", "CTRL+V")) {}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	auto srvDescriptor = m_deviceResources->GetImGuiSRV();
	commandList->SetDescriptorHeaps(1, &srvDescriptor);

	ImGui::Render();
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
	// ImGui	

	PIXEndEvent(commandList);

	// Show the new frame.
	PIXBeginEvent(m_deviceResources->GetCommandQueue(), PIX_COLOR_DEFAULT, L"Present");
	m_deviceResources->Present();
	PIXEndEvent(m_deviceResources->GetCommandQueue());

	m_graphicsMemory->Commit(m_deviceResources->GetCommandQueue());
}

// Helper method to clear the back buffers.
void Game::Clear()
{
	auto commandList = m_deviceResources->GetCommandList();
	PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Clear");

	// Clear the views.
	auto rtvDescriptor = m_deviceResources->GetRenderTargetView();
	auto dsvDescriptor = m_deviceResources->GetDepthStencilView();

	commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &dsvDescriptor);
	commandList->ClearRenderTargetView(rtvDescriptor, Colors::White, 0, nullptr);
	commandList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	// Set the viewport and scissor rect.
	auto viewport = m_deviceResources->GetScreenViewport();
	auto scissorRect = m_deviceResources->GetScissorRect();
	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissorRect);

	PIXEndEvent(commandList);
}
#pragma endregion

#pragma region Message Handlers
// Message handlers
void Game::OnActivated()
{
	// TODO: Game is becoming active window.
}

void Game::OnDeactivated()
{
	// TODO: Game is becoming background window.
}

void Game::OnSuspending()
{
	// TODO: Game is being power-suspended (or minimized).
}

void Game::OnResuming()
{
	m_timer.ResetElapsedTime();

	// TODO: Game is being power-resumed (or returning from minimize).
}

void Game::OnWindowMoved()
{
	auto r = m_deviceResources->GetOutputSize();
	m_deviceResources->WindowSizeChanged(r.right, r.bottom);
}

void Game::OnWindowSizeChanged(int width, int height)
{
	if (!m_deviceResources->WindowSizeChanged(width, height))
		return;

	CreateWindowSizeDependentResources();

	// TODO: Game window is being resized.
}

// Properties
void Game::GetDefaultSize(int& width, int& height) const
{
	// TODO: Change to desired default window size (note minimum size is 320x200).
	width = 800;
	height = 600;
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Game::CreateDeviceDependentResources()
{
	auto device = m_deviceResources->GetD3DDevice();

	// TODO: Initialize device dependent objects here (independent of window size).
	device;
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
	// TODO: Initialize windows-size dependent objects here.
	auto RT_Desc = m_deviceResources->GetRenderTarget()->GetDesc();
	/*m_view = Matrix::CreateLookAt(Vector3(2.f, 2.f, 2.f),
		Vector3::Zero, Vector3::UnitY);*/
	m_proj = Matrix::CreatePerspectiveFieldOfView(XM_PI / 4.f,
		float(RT_Desc.Width) / float(RT_Desc.Height), 0.1f, 500.f);
}

void Game::OnDeviceLost()
{
	// TODO: Add Direct3D resource cleanup here.
	m_states.reset();
	m_fxFactory.reset();
	m_modelResources.reset();
	m_model.reset();
	m_modelNormal.clear();

	m_shape.reset();
	m_effect.reset();

	m_graphicsMemory.reset();
}

void Game::OnDeviceRestored()
{
	CreateDeviceDependentResources();

	CreateWindowSizeDependentResources();
}
#pragma endregion

void Game::SceneParser()
{
	auto railwayList = jsonEngine["Railway"];
	auto mainRW_Data = railwayList[0]["Data"];
	for (auto data : mainRW_Data)
	{
		if (data["Command"] == "Straight")
		{
			int length = data["Parameter"][0];

			/* 新的狀態 */
			Vector3 Pos;
			Vector3 T;
			Vector3 N;
			Vector3 B;

			for (int unit = 0; unit < length; unit++)
			{
				/* 沿著切線前進單位乘上需要的距離 */
				T = currentT;
				T.Normalize();
				Pos = currentPos + T;

				T;
				N = currentN;
				B = -T.Cross(N);

				/* 放置物件 */
				auto world = Matrix(B, N, T);
				world *= Matrix::CreateTranslation(
					Vector3{ Pos.x, Pos.y, Pos.z });

				RailwayDataList.push_back(std::move(world));
				auto tmpPos = Pos;
				RailwayPosList.push_back(std::move(tmpPos));

				/* 重設狀態 */
				currentPos = Pos;
				currentT = T;
				currentN = N;
				currentB = B;
			}			
		}
		else if (data["Command"] == "Curve")
		{
			/* 參數 */
			std::string turn = data["Parameter"][0];       // 左右轉
			int radius = data["Parameter"][1];       // 曲率半徑
			int length = data["Parameter"][2];  // 距離
			float cant = static_cast<float>(data["Parameter"][3]);  // 超高
			float scale = static_cast<float>(data["Parameter"][4]) / 100.f;  // 前進的距離 use scale

			/* 新的狀態 */
			Vector3 Pos = currentPos;
			Vector3 T;
			Vector3 N;
			Vector3 B;
			if (turn=="Right")
			{
				radius = -radius;
			}

			// 利用半徑找到中心點
			// 中心點座標 = 單位B乘上半徑R + 目前的座標
			currentB.Normalize();
			auto centerPos = (currentB * radius) + currentPos;

			// 計算要旋轉的角度						
			float angle = 1.f / radius;

			auto curveStartPos = Pos;

			for (int unit = 0; unit < length; unit++)
			{
				T = currentT;
				N = currentN;
				B = - T.Cross(N);
				
				// 計算位置
				auto rotatePos = centerPos - curveStartPos;
				auto posVector = Vector3::Transform(rotatePos, Matrix::CreateFromAxisAngle(currentN, angle * unit));
				posVector.Normalize();
				if (turn == "Right") {
					Pos = centerPos + (posVector * radius);
				}
				else {
					Pos = centerPos - (posVector * radius);
				}

				// B 指向 centerPos 的水平向量
				B = Pos - centerPos;
				B.Normalize();

				// 取得切線向量
				T = -N.Cross(B);
				T.Normalize();

				// 用切線方向取得超高後的法線向量
				N = Vector3::Transform(N, Matrix::CreateFromAxisAngle(T, sin(cant)));
				N.Normalize();

				// 用法線向量和切線向量取得正確的右手向量
				B = N.Cross(T);
				B.Normalize();				
				
				if (turn == "Left") {
					B = -B;
					T = -T;
				}				

				// 放置物件
				auto world = Matrix(-B, N, -T) * Matrix::Identity;
				world *= Matrix::CreateTranslation(Vector3{ Pos.x, Pos.y, Pos.z });
				
				RailwayDataList.push_back(std::move(world));
				auto tmpPos = Pos;
				RailwayPosList.push_back(std::move(tmpPos));

				/* 重設狀態 */
				currentPos = Pos;
				currentT = T;
				// currentB = B;				
			}			
		}
		else
		{
			MessageBox(hWnd, L"Railway data have invaild command!!", L"ERROR", NULL);
		}
	}

	m_states = std::make_unique<CommonStates>(m_deviceResources->GetD3DDevice());
	m_model = Model::CreateFromSDKMESH(L"Assets/Ballast/ballast.sdkmesh");
	ResourceUploadBatch resourceUpload(m_deviceResources->GetD3DDevice());
	resourceUpload.Begin();
	m_model->LoadStaticBuffers(m_deviceResources->GetD3DDevice(), resourceUpload);

	if (!m_model->textureNames.empty())
	{
		for (auto &texName : m_model->textureNames)
		{
			texName = L"Assets/Ballast/" + texName;
		}
		// m_model->textureNames[0] = outputFile_path + m_model->textureNames[0];
		m_modelResources = m_model->LoadTextures(m_deviceResources->GetD3DDevice(), resourceUpload);

		m_fxFactory = std::make_unique<EffectFactory>(m_modelResources->Heap(), m_states->Heap());

		auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());

		uploadResourcesFinished.wait();

		// RenderTargetState rtState(DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_D32_FLOAT);
		RenderTargetState rtState(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D32_FLOAT);

		EffectPipelineStateDescription pd(
			nullptr,
			CommonStates::Opaque,
			CommonStates::DepthDefault,
			CommonStates::CullClockwise,
			rtState);

		EffectPipelineStateDescription pdAlpha(
			nullptr,
			CommonStates::AlphaBlend,
			CommonStates::DepthDefault,
			CommonStates::CullClockwise,
			rtState);

		m_modelNormal = m_model->CreateEffects(*m_fxFactory, pd, pdAlpha);

		m_world = Matrix::Identity;
	}
	else
	{
		m_model.reset();
		MessageBox(hWnd, L"Model NO Texture!!", L"Error", NULL);
	}

	RWItemUI = true;
}