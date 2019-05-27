//
// Game.cpp
//

#include "pch.h"
#include "Game.h"

extern void ExitGame();

using namespace DirectX;

using Microsoft::WRL::ComPtr;

static HWND hWnd;

Game::Game() noexcept(false)
{
	m_deviceResources = std::make_unique<DX::DeviceResources>();
	m_deviceResources->RegisterDeviceNotify(this);
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

	m_graphicsMemory = std::make_unique<GraphicsMemory>(m_deviceResources->GetD3DDevice());
	/*
	m_states = std::make_unique<CommonStates>(m_deviceResources->GetD3DDevice());

	m_model = Model::CreateFromSDKMESH(L"cup.sdkmesh", m_deviceResources->GetD3DDevice());

	ResourceUploadBatch resourceUpload(m_deviceResources->GetD3DDevice());

	resourceUpload.Begin();

	m_model->LoadStaticBuffers(m_deviceResources->GetD3DDevice(), resourceUpload);

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

	m_world = DirectX::SimpleMath::Matrix::Identity;
	Load_Model = false;
	*/
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

	// Draw Model
	if (m_model != nullptr)
	{
		ID3D12DescriptorHeap* heaps[] = { m_modelResources->Heap(), m_states->Heap() };
		commandList->SetDescriptorHeaps(_countof(heaps), heaps);

		Model::UpdateEffectMatrices(m_modelNormal, m_world, m_view, m_proj);

		m_model->Draw(commandList, m_modelNormal.cbegin());
	}

	// ImGui
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();

	ImGui::NewFrame();
	ImGui::Begin("Main");
	ImGui::Text("FPS: %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
	ImGui::End();

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Convert & Import Model"))
			{
				// Load Model Here!

				std::wstring outputFile;

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
									L".vbo";

								std::wstring cmd =
									std::wstring(std::filesystem::current_path()) +
									L"/tool/meshconvert " +
									std::wstring(pszFilePath) +
									L" -vbo -n -op -o " + 
									outputFile;

								_wsystem(cmd.c_str());								
							}
							psiResult->Release();
						}
					}
				}
				pfd->Release();
				
				m_states = std::make_unique<CommonStates>(m_deviceResources->GetD3DDevice());
			
				// m_model = Model::CreateFromVBO(outputFile.c_str());
				m_model = Model::CreateFromVBO(L"tool\\cup.vbo"); // 因為圖片位置不是在tool\cup.jpg 

				ResourceUploadBatch resourceUpload(m_deviceResources->GetD3DDevice());

				resourceUpload.Begin();

				m_model->LoadStaticBuffers(m_deviceResources->GetD3DDevice(), resourceUpload);

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

				m_world = DirectX::SimpleMath::Matrix::Identity;
				
			}
			if (ImGui::MenuItem("Exit")) { Load_Model = true; }
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
	m_view = DirectX::SimpleMath::Matrix::CreateLookAt(DirectX::SimpleMath::Vector3(2.f, 2.f, 2.f),
		DirectX::SimpleMath::Vector3::Zero, DirectX::SimpleMath::Vector3::UnitY);
	m_proj = DirectX::SimpleMath::Matrix::CreatePerspectiveFieldOfView(XM_PI / 4.f,
		float(RT_Desc.Width) / float(RT_Desc.Height), 0.1f, 10.f);
}

void Game::OnDeviceLost()
{
	// TODO: Add Direct3D resource cleanup here.
	m_states.reset();
	m_fxFactory.reset();
	m_modelResources.reset();
	m_model.reset();
	m_modelNormal.clear();

	m_graphicsMemory.reset();
}

void Game::OnDeviceRestored()
{
	CreateDeviceDependentResources();

	CreateWindowSizeDependentResources();
}
#pragma endregion
