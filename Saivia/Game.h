//
// Game.h
//

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"


// A basic game implementation that creates a D3D12 device and
// provides a game loop.
class Game final : public DX::IDeviceNotify
{
public:

    Game() noexcept(false);
    ~Game();

    // Initialization and management
    void Initialize(HWND window, int width, int height);

    // Basic game loop
    void Tick();

    // IDeviceNotify
    virtual void OnDeviceLost() override;
    virtual void OnDeviceRestored() override;

    // Messages
    void OnActivated();
    void OnDeactivated();
    void OnSuspending();
    void OnResuming();
    void OnWindowMoved();
    void OnWindowSizeChanged(int width, int height);

    // Properties
    void GetDefaultSize( int& width, int& height ) const;

private:

    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

	// Parser
	void SceneParser();

    // Device resources.
    std::unique_ptr<DX::DeviceResources>    m_deviceResources;

    // Rendering loop timer.
    DX::StepTimer                           m_timer;

	// Necessary for DXTK12 !!
	std::unique_ptr<DirectX::GraphicsMemory> m_graphicsMemory;

	// Model
	DirectX::SimpleMath::Matrix m_world;
	DirectX::SimpleMath::Matrix m_view;
	// DirectX::SimpleMath::Matrix m_proj;

	std::unique_ptr<DirectX::CommonStates> m_states;
	std::unique_ptr<DirectX::EffectFactory> m_fxFactory;
	std::unique_ptr<DirectX::EffectTextureFactory> m_modelResources;
	std::unique_ptr<DirectX::Model> m_model;
	std::vector<std::shared_ptr<DirectX::IEffect>> m_modelNormal;

	// ImGui
	bool ModelUI = false;

	// json
	nlohmann::json jsonEngine;

	// Controller
	std::unique_ptr<DirectX::Keyboard> m_keyboard;
	std::unique_ptr<DirectX::Mouse> m_mouse;

	// Camera
	DirectX::SimpleMath::Matrix m_proj;
	DirectX::SimpleMath::Vector3 m_cameraPos;
	float m_pitch;
	float m_yaw;
	bool isCameraLock = false; // Lock on Railway

	// Railway

	std::vector<DirectX::SimpleMath::Matrix> RailwayDataList;
	std::vector<DirectX::SimpleMath::Vector3> RailwayPosList;

	DirectX::SimpleMath::Vector3 currentPos;
	DirectX::SimpleMath::Vector3 currentT;
	DirectX::SimpleMath::Vector3 currentB;
	DirectX::SimpleMath::Vector3 currentN;

	bool RWItemUI = false;

	// reference position Geometric
	std::unique_ptr<DirectX::GeometricPrimitive> m_shape;
	std::unique_ptr<DirectX::BasicEffect> m_effect;
};
