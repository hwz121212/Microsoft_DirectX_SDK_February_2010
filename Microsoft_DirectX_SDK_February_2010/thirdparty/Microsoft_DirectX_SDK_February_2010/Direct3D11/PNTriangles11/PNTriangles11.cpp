//--------------------------------------------------------------------------------------
// File: PNTriangles11.cpp
//
// This code sample demonstrates the use of DX11 Hull & Domain shaders to implement the 
// PN-Triangles tessellation technique
//
// Contributed by the AMD Developer Relations Team
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTgui.h"
#include "DXUTsettingsdlg.h"
#include "SDKmisc.h"
#include "SDKMesh.h"
#include <D3DX11tex.h>
#include <D3DX11.h>
#include <D3DX11core.h>
#include <D3DX11async.h>

// The different meshes
typedef enum _MESH_TYPE
{
    MESH_TYPE_TINY      = 0,
    MESH_TYPE_TIGER     = 1,
    MESH_TYPE_TEAPOT    = 2,
    MESH_TYPE_USER      = 3,
    MESH_TYPE_MAX       = 4,
}MESH_TYPE;

CDXUTDialogResourceManager  g_DialogResourceManager;    // Manager for shared resources of dialogs
CModelViewerCamera          g_Camera[MESH_TYPE_MAX];    // A model viewing camera for each mesh scene
CModelViewerCamera          g_LightCamera;              // A model viewing camera for the light
CD3DSettingsDlg             g_D3DSettingsDlg;           // Device settings dialog
CDXUTDialog                 g_HUD;                      // Dialog for standard controls
CDXUTDialog                 g_SampleUI;                 // Dialog for sample specific controls
CDXUTTextHelper*            g_pTxtHelper = NULL;

// The scene meshes 
CDXUTSDKMesh                g_SceneMesh[MESH_TYPE_MAX];
static ID3D11InputLayout*   g_pSceneVertexLayout = NULL;
MESH_TYPE                   g_eMeshType = MESH_TYPE_TINY;
D3DXMATRIX                  g_m4x4MeshMatrix[MESH_TYPE_MAX];
D3DXVECTOR2                 g_v2AdaptiveTessParams[MESH_TYPE_MAX];

// Samplers
ID3D11SamplerState*         g_pSamplePoint = NULL;
ID3D11SamplerState*         g_pSampleLinear = NULL;

// Shaders
ID3D11VertexShader*         g_pSceneVS = NULL;
ID3D11VertexShader*         g_pSceneWithTessellationVS = NULL;
ID3D11HullShader*           g_pPNTrianglesHS = NULL;
ID3D11HullShader*			g_pPNTrianglesAdaptiveHS = NULL;
ID3D11DomainShader*         g_pPNTrianglesDS = NULL;
ID3D11PixelShader*          g_pScenePS = NULL;
ID3D11PixelShader*          g_pTexturedScenePS = NULL;

// Constant buffer layout for transfering data to the PN-Triangles HLSL functions
struct CB_PNTRIANGLES
{
    D3DXMATRIX f4x4World;               // World matrix for object
    D3DXMATRIX f4x4ViewProjection;      // View * Projection matrix
    D3DXMATRIX f4x4WorldViewProjection; // World * View * Projection matrix  
    float fLightDir[4];                 // Light direction vector
    float fEye[4];                      // Eye
    float fTessFactors[4];              // Tessellation factors ( x=Edge, y=Inside, z=MinDistance, w=Range )
};
UINT                    g_iPNTRIANGLESCBBind = 0;

// Various Constant buffers
static ID3D11Buffer*    g_pcbPNTriangles = NULL;                 

// State objects
ID3D11RasterizerState*  g_pRasterizerStateWireframe = NULL;
ID3D11RasterizerState*  g_pRasterizerStateSolid = NULL;

// Capture texture
static ID3D11Texture2D*    g_pCaptureTexture = NULL;

// User supplied data
static bool g_bUserMesh = false;
static ID3D11ShaderResourceView* g_pDiffuseTextureSRV = NULL;

// Tess factor
static unsigned int g_uTessFactor = 5;

// Cmd line params
typedef struct _CmdLineParams
{
    D3D_DRIVER_TYPE DriverType;
    unsigned int uWidth;
    unsigned int uHeight;
    bool bCapture;
    WCHAR strCaptureFilename[256];
    int iExitFrame;
    bool bRenderHUD;
}CmdLineParams;
static CmdLineParams g_CmdLineParams;


//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------

// Standard device control
#define IDC_TOGGLEFULLSCREEN        1
#define IDC_TOGGLEREF               3
#define IDC_CHANGEDEVICE            4

// Sample UI
#define IDC_STATIC_MESH             5
#define IDC_COMBOBOX_MESH           6
#define IDC_CHECKBOX_WIREFRAME      7
#define IDC_CHECKBOX_TEXTURED       8
#define IDC_CHECKBOX_TESSELLATION   9
#define IDC_CHECKBOX_ADAPTIVE       10
#define IDC_STATIC_TESS_FACTOR      11
#define IDC_SLIDER_TESS_FACTOR      12


//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                         void* pUserContext );
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );

bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                      DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext );
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                     void* pUserContext );
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                         const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext );
void CALLBACK OnD3D11DestroyDevice( void* pUserContext );
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                 float fElapsedTime, void* pUserContext );

void InitApp();
void RenderText();

// Helper functions
HRESULT CompileShaderFromFile( WCHAR* szFileName, D3D10_SHADER_MACRO* pDefines, LPCSTR szEntryPoint, 
                               LPCSTR szShaderModel, ID3DBlob** ppBlobOut );
bool IsNextArg( WCHAR*& strCmdLine, WCHAR* strArg );
bool GetCmdParam( WCHAR*& strCmdLine, WCHAR* strFlag );
void ParseCommandLine();
HRESULT CreateSurface( ID3D11Texture2D** ppTexture, ID3D11ShaderResourceView** ppTextureSRV,
                       ID3D11RenderTargetView** ppTextureRTV, DXGI_FORMAT Format, unsigned int uWidth,
                       unsigned int uHeight );
void CaptureFrame();
void RenderMesh( CDXUTSDKMesh* pDXUTMesh, UINT uMesh, 
                 D3D11_PRIMITIVE_TOPOLOGY PrimType = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED, 
                 UINT uDiffuseSlot = INVALID_SAMPLER_SLOT, UINT uNormalSlot = INVALID_SAMPLER_SLOT,
                 UINT uSpecularSlot = INVALID_SAMPLER_SLOT );
bool FileExists( WCHAR* pFileName );


//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow )
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    // Disable gamma correction on this sample
    DXUTSetIsInGammaCorrectMode( false );

    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackKeyboard( OnKeyboard );
    DXUTSetCallbackFrameMove( OnFrameMove );
    
    DXUTSetCallbackD3D11DeviceAcceptable( IsD3D11DeviceAcceptable );
    DXUTSetCallbackD3D11DeviceCreated( OnD3D11CreateDevice );
    DXUTSetCallbackD3D11SwapChainResized( OnD3D11ResizedSwapChain );
    DXUTSetCallbackD3D11FrameRender( OnD3D11FrameRender );
    DXUTSetCallbackD3D11SwapChainReleasing( OnD3D11ReleasingSwapChain );
    DXUTSetCallbackD3D11DeviceDestroyed( OnD3D11DestroyDevice );

    ParseCommandLine();

    InitApp();
    
    //DXUTInit( true, true, L"-forceref" ); // Force create a ref device so that feature level D3D_FEATURE_LEVEL_11_0 is guaranteed
    DXUTInit( true, true );                 // Use this line instead to try to create a hardware device

    DXUTSetCursorSettings( true, true );    // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"PN-Triangles Direct3D 11" );
    DXUTCreateDevice( D3D_FEATURE_LEVEL_11_0, true, g_CmdLineParams.uWidth, g_CmdLineParams.uHeight );
    DXUTMainLoop();                         // Enter into the DXUT render loop

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
    g_D3DSettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );
    g_SampleUI.GetFont( 0 );

    g_HUD.SetCallback( OnGUIEvent ); 
    int iY = 30;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 0, iY, 170, 23 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 0, iY += 26, 170, 23, VK_F3 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 0, iY += 26, 170, 23, VK_F2 );

    g_SampleUI.SetCallback( OnGUIEvent );
    iY = 0;
    g_SampleUI.AddStatic( IDC_STATIC_MESH, L"Mesh:", 0, iY, 55, 24 );
    CDXUTComboBox *pCombo;
    g_SampleUI.AddComboBox( IDC_COMBOBOX_MESH, 0, iY += 25, 140, 24, 0, true, &pCombo );
    if( pCombo )
    {
        pCombo->SetDropHeight( 45 );
        pCombo->AddItem( L"Tiny", NULL );
        pCombo->AddItem( L"Tiger", NULL );
        pCombo->AddItem( L"Teapot", NULL );
        pCombo->AddItem( L"User", NULL );
        pCombo->SetSelectedByIndex( 0 );
    }
    g_SampleUI.AddCheckBox( IDC_CHECKBOX_WIREFRAME, L"Wireframe", 0, iY += 25, 140, 24, false );
    g_SampleUI.AddCheckBox( IDC_CHECKBOX_TEXTURED, L"Textured", 0, iY += 25, 140, 24, true );
    g_SampleUI.AddCheckBox( IDC_CHECKBOX_TESSELLATION, L"Tessellation", 0, iY += 25, 140, 24, false );
    g_SampleUI.AddCheckBox( IDC_CHECKBOX_ADAPTIVE, L"Adaptive", 0, iY += 25, 140, 24, false );
    WCHAR szTemp[256];
    swprintf_s( szTemp, L"Tess Factor : %d", g_uTessFactor );
    g_SampleUI.AddStatic( IDC_STATIC_TESS_FACTOR, szTemp, 5, iY += 25, 108, 24 );
    g_SampleUI.AddSlider( IDC_SLIDER_TESS_FACTOR, 0, iY += 25, 140, 24, 1, 10, 1 + ( g_uTessFactor - 1 ) / 2, false );
}


//--------------------------------------------------------------------------------------
// This callback function is called immediately before a device is created to allow the 
// application to modify the device settings. The supplied pDeviceSettings parameter 
// contains the settings that the framework has selected for the new device, and the 
// application can make any desired changes directly to this structure.  Note however that 
// DXUT will not correct invalid device settings so care must be taken 
// to return valid device settings, otherwise CreateDevice() will fail.  
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    assert( pDeviceSettings->ver == DXUT_D3D11_DEVICE );

    // For the first device created if it is a REF device, optionally display a warning dialog box
    static bool s_bFirstTime = true;
    if( s_bFirstTime )
    {
        s_bFirstTime = false;

        // Set driver type based on cmd line / program default
        pDeviceSettings->d3d11.DriverType = g_CmdLineParams.DriverType;

        // Disable vsync
        pDeviceSettings->d3d11.SyncInterval = 0;

        if( ( DXUT_D3D9_DEVICE == pDeviceSettings->ver && pDeviceSettings->d3d9.DeviceType == D3DDEVTYPE_REF ) ||
            ( DXUT_D3D11_DEVICE == pDeviceSettings->ver &&
            pDeviceSettings->d3d11.DriverType == D3D_DRIVER_TYPE_REFERENCE ) )
        {
            DXUTDisplaySwitchingToREFWarning( pDeviceSettings->ver );
        }
    }

    return true;
}


//--------------------------------------------------------------------------------------
// This callback function will be called once at the beginning of every frame. This is the
// best location for your application to handle updates to the scene, but is not 
// intended to contain actual rendering calls, which should instead be placed in the 
// OnFrameRender callback.  
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    // Update the camera's position based on user input 
    g_Camera[g_eMeshType].FrameMove( fElapsedTime );
    g_LightCamera.FrameMove( fElapsedTime );
}


//--------------------------------------------------------------------------------------
// Render stats
//--------------------------------------------------------------------------------------
void RenderText()
{
    g_pTxtHelper->Begin();
    
    g_pTxtHelper->SetInsertionPos( 2, 0 );
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );

    g_pTxtHelper->SetInsertionPos( 2, DXUTGetDXGIBackBufferSurfaceDesc()->Height - 35 );
    g_pTxtHelper->DrawTextLine( L"Toggle GUI    : G" );
    g_pTxtHelper->DrawTextLine( L"Frame Capture : C" );

    g_pTxtHelper->End();
}


//--------------------------------------------------------------------------------------
// Before handling window messages, DXUT passes incoming windows 
// messages to the application through this callback function. If the application sets 
// *pbNoFurtherProcessing to TRUE, then DXUT will not process this message.
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                         void* pUserContext )
{
    // Pass messages to dialog resource manager calls so GUI state is updated correctly
    *pbNoFurtherProcessing = g_DialogResourceManager.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass messages to settings dialog if its active
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.MsgProc( hWnd, uMsg, wParam, lParam );
        return 0;
    }

    // Give the dialogs a chance to handle the message first
    *pbNoFurtherProcessing = g_HUD.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;
    *pbNoFurtherProcessing = g_SampleUI.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass all windows messages to camera so it can respond to user input
    g_Camera[g_eMeshType].HandleMessages( hWnd, uMsg, wParam, lParam );
    g_LightCamera.HandleMessages( hWnd, uMsg, wParam, lParam );

    return 0;
}


//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
    WCHAR szTemp[256];
    
    switch( nControlID )
    {
        case IDC_TOGGLEFULLSCREEN:
            DXUTToggleFullScreen();
            break;

        case IDC_TOGGLEREF:
            DXUTToggleREF();
            break;

        case IDC_CHANGEDEVICE:
            g_D3DSettingsDlg.SetActive( !g_D3DSettingsDlg.IsActive() );
            break;

        case IDC_SLIDER_TESS_FACTOR:
            g_uTessFactor = ( (unsigned int)((CDXUTSlider*)pControl)->GetValue() - 1 ) * 2 + 1;
            swprintf_s( szTemp, L"Tess Factor : %d", g_uTessFactor );
            g_SampleUI.GetStatic( IDC_STATIC_TESS_FACTOR )->SetText( szTemp );
            break;

        case IDC_COMBOBOX_MESH:
            g_eMeshType = (MESH_TYPE)((CDXUTComboBox*)pControl)->GetSelectedIndex();
            if( MESH_TYPE_USER == g_eMeshType )
            {
                g_SampleUI.GetCheckBox( IDC_CHECKBOX_TEXTURED )->SetEnabled( ( g_pDiffuseTextureSRV != NULL ) ? ( true ) : ( false ) );
                g_SampleUI.GetCheckBox( IDC_CHECKBOX_TEXTURED )->SetChecked( ( g_pDiffuseTextureSRV != NULL ) ? ( g_SampleUI.GetCheckBox( IDC_CHECKBOX_TEXTURED )->GetChecked() ) : ( false ) );
            }
            else
            {
                if( MESH_TYPE_TEAPOT == g_eMeshType )
                {
                    g_SampleUI.GetCheckBox( IDC_CHECKBOX_TEXTURED )->SetEnabled( false );
                }
                else
                {
                    g_SampleUI.GetCheckBox( IDC_CHECKBOX_TEXTURED )->SetEnabled( true );
                }
            }
            break;
    }
}


//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
    static int iCaptureNumber = 0;
    #define VK_C (67)
    #define VK_G (71)

    if( bKeyDown )
    {
        switch( nChar )
        {
            case VK_C:    
                swprintf_s( g_CmdLineParams.strCaptureFilename, L"FrameCapture%d.bmp", iCaptureNumber );
                CaptureFrame();
                iCaptureNumber++;
                break;

            case VK_G:
                g_CmdLineParams.bRenderHUD = !g_CmdLineParams.bRenderHUD;
                break;
        }
    }
}


//--------------------------------------------------------------------------------------
// Reject any D3D11 devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                      DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    return true;
}


//--------------------------------------------------------------------------------------
// This callback function will be called immediately after the Direct3D device has been 
// created, which will happen during application initialization and windowed/full screen 
// toggles. This is the best location to create D3DPOOL_MANAGED resources since these 
// resources need to be reloaded whenever the device is destroyed. Resources created  
// here should be released in the OnD3D11DestroyDevice callback. 
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                     void* pUserContext )
{
    HRESULT hr;

    static bool bFirstOnCreateDevice = true;

    // Warn the user that in order to support DX11, a non-hardware device has been created, continue or quit?
    if ( DXUTGetDeviceSettings().d3d11.DriverType != D3D_DRIVER_TYPE_HARDWARE && bFirstOnCreateDevice )
    {
        if ( MessageBox( 0, L"No Direct3D 11 hardware detected. "\
                            L"In order to continue, a non-hardware device has been created, "\
                            L"it will be very slow, continue?", L"Warning", MB_ICONEXCLAMATION | MB_YESNO ) != IDYES )
            return E_FAIL;
    }
    
    bFirstOnCreateDevice = false;

    ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();
    V_RETURN( g_DialogResourceManager.OnD3D11CreateDevice( pd3dDevice, pd3dImmediateContext ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D11CreateDevice( pd3dDevice ) );
    g_pTxtHelper = new CDXUTTextHelper( pd3dDevice, pd3dImmediateContext, &g_DialogResourceManager, 15 );

    // Create the shaders
    ID3DBlob* pBlob = NULL;
    D3D10_SHADER_MACRO ShaderMacros[2];
	ShaderMacros[0].Definition = "1";
	ShaderMacros[1].Definition = "1";
    
    // Main scene VS (no tessellation)
    V_RETURN( CompileShaderFromFile( L"PNTriangles11.hlsl", NULL, "VS_RenderScene", "vs_4_0", &pBlob ) ); 
    V_RETURN( pd3dDevice->CreateVertexShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pSceneVS ) );
    
    // Define our scene vertex data layout
    const D3D11_INPUT_ELEMENT_DESC SceneLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    V_RETURN( pd3dDevice->CreateInputLayout( SceneLayout, ARRAYSIZE( SceneLayout ), pBlob->GetBufferPointer(),
                                                pBlob->GetBufferSize(), &g_pSceneVertexLayout ) );
    SAFE_RELEASE( pBlob );

    // Main scene VS (with tessellation)
    V_RETURN( CompileShaderFromFile( L"PNTriangles11.hlsl", NULL, "VS_RenderSceneWithTessellation", "vs_4_0", &pBlob ) ); 
    V_RETURN( pd3dDevice->CreateVertexShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pSceneWithTessellationVS ) );

    // PNTriangles HS
    V_RETURN( CompileShaderFromFile( L"PNTriangles11.hlsl", NULL, "HS_PNTriangles", "hs_5_0", &pBlob ) ); 
    V_RETURN( pd3dDevice->CreateHullShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pPNTrianglesHS ) );

    // PNTriangles HS (Adaptive)
    ShaderMacros[0].Name = "USE_ADAPTIVE_TESSELLATION";
    ShaderMacros[1].Name = NULL;
    V_RETURN( CompileShaderFromFile( L"PNTriangles11.hlsl", ShaderMacros, "HS_PNTriangles", "hs_5_0", &pBlob ) ); 
	V_RETURN( pd3dDevice->CreateHullShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pPNTrianglesAdaptiveHS ) );
    
    // PNTriangles DS
    V_RETURN( CompileShaderFromFile( L"PNTriangles11.hlsl", NULL, "DS_PNTriangles", "ds_5_0", &pBlob ) ); 
    V_RETURN( pd3dDevice->CreateDomainShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pPNTrianglesDS ) );

    // Main scene PS (no textures)
    V_RETURN( CompileShaderFromFile( L"PNTriangles11.hlsl", NULL, "PS_RenderScene", "ps_4_0", &pBlob ) ); 
    V_RETURN( pd3dDevice->CreatePixelShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pScenePS ) );
    
    // Main scene PS (textured)
    V_RETURN( CompileShaderFromFile( L"PNTriangles11.hlsl", NULL, "PS_RenderSceneTextured", "ps_4_0", &pBlob ) ); 
    V_RETURN( pd3dDevice->CreatePixelShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pTexturedScenePS ) );
    SAFE_RELEASE( pBlob );
    
    // Setup constant buffer
    D3D11_BUFFER_DESC Desc;
    Desc.Usage = D3D11_USAGE_DYNAMIC;
    Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Desc.MiscFlags = 0;    
    Desc.ByteWidth = sizeof( CB_PNTRIANGLES );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &g_pcbPNTriangles ) );

    // Setup the camera for each scene   
    D3DXVECTOR3 vecEye( 0.0f, 0.0f, 0.0f );
    D3DXVECTOR3 vecAt ( 0.0f, 0.0f, 0.0f );
    // Tiny
    vecEye.x = 0.0f; vecEye.y = 0.0f; vecEye.z = -700.0f;
    g_Camera[MESH_TYPE_TINY].SetViewParams( &vecEye, &vecAt );
    // Tiger
    vecEye.x = 0.0f; vecEye.y = 0.0f; vecEye.z = -4.0f;
    g_Camera[MESH_TYPE_TIGER].SetViewParams( &vecEye, &vecAt );
    // Teapot
    vecEye.x = 0.0f; vecEye.y = 0.0f; vecEye.z = -4.0f;
    g_Camera[MESH_TYPE_TEAPOT].SetViewParams( &vecEye, &vecAt );
    // User
    vecEye.x = 0.0f; vecEye.y = 0.0f; vecEye.z = -3.0f;
    g_Camera[MESH_TYPE_USER].SetViewParams( &vecEye, &vecAt );

    // Setup the mesh params for adaptive tessellation
    g_v2AdaptiveTessParams[MESH_TYPE_TINY].x    = 700.0f;
    g_v2AdaptiveTessParams[MESH_TYPE_TINY].y    = 2000.0f - g_v2AdaptiveTessParams[MESH_TYPE_TINY].x;
    g_v2AdaptiveTessParams[MESH_TYPE_TIGER].x   = 1.0f;
    g_v2AdaptiveTessParams[MESH_TYPE_TIGER].y   = 10.0f - g_v2AdaptiveTessParams[MESH_TYPE_TIGER].x;
    g_v2AdaptiveTessParams[MESH_TYPE_TEAPOT].x  = 1.0f;
    g_v2AdaptiveTessParams[MESH_TYPE_TEAPOT].y  = 10.0f - g_v2AdaptiveTessParams[MESH_TYPE_TEAPOT].x;
    g_v2AdaptiveTessParams[MESH_TYPE_USER].x    = 1.0f;
    g_v2AdaptiveTessParams[MESH_TYPE_USER].y    = 10.0f - g_v2AdaptiveTessParams[MESH_TYPE_USER].x;

    // Setup the matrix for each mesh
    D3DXMATRIXA16 mModelRotationX;
    D3DXMATRIXA16 mModelRotationY;
    D3DXMATRIXA16 mModelTranslation;
    // Tiny
    D3DXMatrixRotationX( &mModelRotationX, -D3DX_PI / 2 ); 
    D3DXMatrixRotationY( &mModelRotationY, D3DX_PI ); 
    g_m4x4MeshMatrix[MESH_TYPE_TINY] = mModelRotationX * mModelRotationY;
    // Tiger
    D3DXMatrixRotationX( &mModelRotationX, -D3DX_PI / 36 ); 
    D3DXMatrixRotationY( &mModelRotationY, D3DX_PI / 4 ); 
    g_m4x4MeshMatrix[MESH_TYPE_TIGER] = mModelRotationX * mModelRotationY;
    // Teapot
    D3DXMatrixIdentity( &g_m4x4MeshMatrix[MESH_TYPE_TEAPOT] );
    // User
    D3DXMatrixTranslation( &mModelTranslation, 0.0f, -1.0f, 0.0f );
    g_m4x4MeshMatrix[MESH_TYPE_USER] = mModelTranslation;

    // Setup the light camera
    vecEye.x = 0.0f; vecEye.y = -1.0f; vecEye.z = -1.0f;
    g_LightCamera.SetViewParams( &vecEye, &vecAt );

    // Load the standard scene meshes
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"tiny\\tiny.sdkmesh" ) );
    hr = g_SceneMesh[MESH_TYPE_TINY].Create( pd3dDevice, str );
    assert( D3D_OK == hr );

    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH,  L"tiger\\tiger.sdkmesh" ) );
    hr = g_SceneMesh[MESH_TYPE_TIGER].Create( pd3dDevice, str );
    assert( D3D_OK == hr );

    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"teapot\\teapot.sdkmesh" ) );
    hr = g_SceneMesh[MESH_TYPE_TEAPOT].Create( pd3dDevice, str );
    assert( D3D_OK == hr );
    
    // Load a user mesh and textures if present
    g_bUserMesh = false;
    g_pDiffuseTextureSRV = NULL;
    // The mesh

    str[0] = 0;
    hr = DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"user.sdkmesh" ) ;
    if( FileExists( str ) )
    {
        hr = g_SceneMesh[MESH_TYPE_USER].Create( pd3dDevice, str );
        assert( D3D_OK == hr );
        g_bUserMesh = true;
    }
    // The user textures
    if( FileExists( L"media\\user\\diffuse.dds" ) )
    {
        hr = D3DX11CreateShaderResourceViewFromFile( pd3dDevice, L"media\\user\\diffuse.dds", NULL, NULL, &g_pDiffuseTextureSRV, NULL );
        assert( D3D_OK == hr );
    }
                        
    // Create sampler states for point and linear
    // Point
    D3D11_SAMPLER_DESC SamDesc;
    SamDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    SamDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamDesc.MipLODBias = 0.0f;
    SamDesc.MaxAnisotropy = 1;
    SamDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    SamDesc.BorderColor[0] = SamDesc.BorderColor[1] = SamDesc.BorderColor[2] = SamDesc.BorderColor[3] = 0;
    SamDesc.MinLOD = 0;
    SamDesc.MaxLOD = D3D11_FLOAT32_MAX;
    V_RETURN( pd3dDevice->CreateSamplerState( &SamDesc, &g_pSamplePoint ) );
    // Linear
    SamDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    SamDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    SamDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    SamDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    V_RETURN( pd3dDevice->CreateSamplerState( &SamDesc, &g_pSampleLinear ) );

    // Set the raster state
    // Wireframe
    D3D11_RASTERIZER_DESC RasterizerDesc;
    RasterizerDesc.FillMode = D3D11_FILL_WIREFRAME;
    RasterizerDesc.CullMode = D3D11_CULL_BACK;
    RasterizerDesc.FrontCounterClockwise = FALSE;
    RasterizerDesc.DepthBias = 0;
    RasterizerDesc.DepthBiasClamp = 0.0f;
    RasterizerDesc.SlopeScaledDepthBias = 0.0f;
    RasterizerDesc.DepthClipEnable = TRUE;
    RasterizerDesc.ScissorEnable = FALSE;
    RasterizerDesc.MultisampleEnable = FALSE;
    RasterizerDesc.AntialiasedLineEnable = FALSE;
    V_RETURN( pd3dDevice->CreateRasterizerState( &RasterizerDesc, &g_pRasterizerStateWireframe ) );
    // Solid
    RasterizerDesc.FillMode = D3D11_FILL_SOLID;
    V_RETURN( pd3dDevice->CreateRasterizerState( &RasterizerDesc, &g_pRasterizerStateSolid ) );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Resize
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                         const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

    // Setup the camera's projection parameters    
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    for( int iMeshType=0; iMeshType<MESH_TYPE_MAX; iMeshType++ )
    {
        g_Camera[iMeshType].SetProjParams( D3DX_PI / 4, fAspectRatio, 0.1f, 5000.0f );
        g_Camera[iMeshType].SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
        g_Camera[iMeshType].SetButtonMasks( MOUSE_MIDDLE_BUTTON, MOUSE_WHEEL, MOUSE_LEFT_BUTTON );
    }

    // Setup the light camera's projection params
    g_LightCamera.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.1f, 5000.0f );
    g_LightCamera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_LightCamera.SetButtonMasks( MOUSE_RIGHT_BUTTON, MOUSE_WHEEL, MOUSE_RIGHT_BUTTON );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 220 );
    g_SampleUI.SetSize( 150, 110 );

    // We need a screen-sized STAGING resource for frame capturing
    D3D11_TEXTURE2D_DESC TexDesc;
    DXGI_SAMPLE_DESC SingleSample = { 1, 0 };
    TexDesc.Width = pBackBufferSurfaceDesc->Width;
    TexDesc.Height = pBackBufferSurfaceDesc->Height;
    TexDesc.Format = pBackBufferSurfaceDesc->Format;
    TexDesc.SampleDesc = SingleSample;
    TexDesc.MipLevels = 1;
    TexDesc.Usage = D3D11_USAGE_STAGING;
    TexDesc.MiscFlags = 0;
    TexDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    TexDesc.BindFlags = 0;
    TexDesc.ArraySize = 1;
    hr = pd3dDevice->CreateTexture2D( &TexDesc, NULL, &g_pCaptureTexture );
    assert( D3D_OK == hr );
    
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Debug function which copies a GPU buffer to a CPU readable buffer
//--------------------------------------------------------------------------------------
ID3D11Buffer* CreateAndCopyToDebugBuf( ID3D11Device* pDevice, ID3D11DeviceContext* pd3dImmediateContext, ID3D11Buffer* pBuffer )
{
    ID3D11Buffer* debugbuf = NULL;

    D3D11_BUFFER_DESC desc;
    ZeroMemory( &desc, sizeof(desc) );
    pBuffer->GetDesc( &desc );
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.MiscFlags = 0;
    pDevice->CreateBuffer(&desc, NULL, &debugbuf);

    pd3dImmediateContext->CopyResource( debugbuf, pBuffer );
    
    return debugbuf;

}


//--------------------------------------------------------------------------------------
// Render
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                 float fElapsedTime, void* pUserContext )
{
    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.OnRender( fElapsedTime );
        return;
    }

    // Array of our samplers
    ID3D11SamplerState* ppSamplerStates[2] = { g_pSamplePoint, g_pSampleLinear };

    // Clear the render target & depth stencil
    float ClearColor[4] = { 0.176f, 0.196f, 0.667f, 0.0f };
    pd3dImmediateContext->ClearRenderTargetView( DXUTGetD3D11RenderTargetView(), ClearColor );
    pd3dImmediateContext->ClearDepthStencilView( DXUTGetD3D11DepthStencilView(), D3D11_CLEAR_DEPTH, 1.0, 0 );    
    
    // Get the projection & view matrix from the camera class
    D3DXMATRIXA16 mWorld;
    D3DXMATRIXA16 mView;
    D3DXMATRIXA16 mProj;
    D3DXMATRIXA16 mWorldViewProjection;
    D3DXMATRIXA16 mViewProjection;
    mWorld = *g_Camera[g_eMeshType].GetWorldMatrix();
    mView = *g_Camera[g_eMeshType].GetViewMatrix();
    mProj = *g_Camera[g_eMeshType].GetProjMatrix();
    mWorldViewProjection = g_m4x4MeshMatrix[g_eMeshType] * mWorld * mView * mProj;
    mViewProjection = g_m4x4MeshMatrix[g_eMeshType] * mView * mProj;
    
    // Get the direction of the light.
    D3DXVECTOR3 v3LightDir = *g_LightCamera.GetEyePt() - *g_LightCamera.GetLookAtPt();
    D3DXVec3Normalize( &v3LightDir, &v3LightDir );

    // Setup the constant buffer for the scene vertex shader
    D3D11_MAPPED_SUBRESOURCE MappedResource;
    pd3dImmediateContext->Map( g_pcbPNTriangles, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
    CB_PNTRIANGLES* pPNTrianglesCB = ( CB_PNTRIANGLES* )MappedResource.pData;
    D3DXMatrixTranspose( &pPNTrianglesCB->f4x4World, &mWorld );
    D3DXMatrixTranspose( &pPNTrianglesCB->f4x4ViewProjection, &mViewProjection );
    D3DXMatrixTranspose( &pPNTrianglesCB->f4x4WorldViewProjection, &mWorldViewProjection );
    pPNTrianglesCB->fLightDir[0] = v3LightDir.x; 
    pPNTrianglesCB->fLightDir[1] = v3LightDir.y; 
    pPNTrianglesCB->fLightDir[2] = v3LightDir.z; 
    pPNTrianglesCB->fLightDir[3] = 0.0f;
    pPNTrianglesCB->fEye[0] = g_Camera[g_eMeshType].GetEyePt()->x;
    pPNTrianglesCB->fEye[1] = g_Camera[g_eMeshType].GetEyePt()->y;
    pPNTrianglesCB->fEye[2] = g_Camera[g_eMeshType].GetEyePt()->z;
    pPNTrianglesCB->fTessFactors[0] = (float)g_uTessFactor;
    pPNTrianglesCB->fTessFactors[1] = (float)g_uTessFactor;
    pPNTrianglesCB->fTessFactors[2] = (float)g_v2AdaptiveTessParams[g_eMeshType].x;
    pPNTrianglesCB->fTessFactors[3] = (float)g_v2AdaptiveTessParams[g_eMeshType].y;
    pd3dImmediateContext->Unmap( g_pcbPNTriangles, 0 );
    pd3dImmediateContext->VSSetConstantBuffers( g_iPNTRIANGLESCBBind, 1, &g_pcbPNTriangles );
     pd3dImmediateContext->PSSetConstantBuffers( g_iPNTRIANGLESCBBind, 1, &g_pcbPNTriangles );

    // Based on app and GUI settings set a bunch of bools that guide the render
    bool bTessellation = g_SampleUI.GetCheckBox( IDC_CHECKBOX_TESSELLATION )->GetChecked();
    bool bTextured = g_SampleUI.GetCheckBox( IDC_CHECKBOX_TEXTURED )->GetChecked() && g_SampleUI.GetCheckBox( IDC_CHECKBOX_TEXTURED )->GetEnabled();
    bool bAdaptive = g_SampleUI.GetCheckBox( IDC_CHECKBOX_ADAPTIVE )->GetChecked();
    
    // VS
    if( bTessellation )
    {
        pd3dImmediateContext->VSSetShader( g_pSceneWithTessellationVS, NULL, 0 );
    }
    else
    {
        pd3dImmediateContext->VSSetShader( g_pSceneVS, NULL, 0 );
    }
    pd3dImmediateContext->IASetInputLayout( g_pSceneVertexLayout );

    // HS
    ID3D11HullShader*    pHS = NULL;
    if( bTessellation )
    {
        pd3dImmediateContext->HSSetConstantBuffers( g_iPNTRIANGLESCBBind, 1, &g_pcbPNTriangles );
        pHS = g_pPNTrianglesHS;
        if( bAdaptive )
        {
            pHS = g_pPNTrianglesAdaptiveHS;
        }
        
    }
    pd3dImmediateContext->HSSetShader( pHS, NULL, 0 );
    
    // DS
    ID3D11DomainShader*    pDS = NULL;
    if( bTessellation )
    {
        pd3dImmediateContext->DSSetConstantBuffers( g_iPNTRIANGLESCBBind, 1, &g_pcbPNTriangles );
        pDS = g_pPNTrianglesDS;
    }
    pd3dImmediateContext->DSSetShader( pDS, NULL, 0 );
    
    // GS
    pd3dImmediateContext->GSSetShader( NULL, NULL, 0 );

    // PS
    ID3D11PixelShader*    pPS = NULL;
    if( bTextured )
    {
        pd3dImmediateContext->PSSetSamplers( 0, 2, ppSamplerStates );
        pd3dImmediateContext->PSSetShaderResources( 0, 1, &g_pDiffuseTextureSRV );
        pPS = g_pTexturedScenePS;
    }
    else
    {
        pPS = g_pScenePS;
    }
    pd3dImmediateContext->PSSetShader( pPS, NULL, 0 );

    // Set the rasterizer state
    if( g_SampleUI.GetCheckBox( IDC_CHECKBOX_WIREFRAME )->GetChecked() )
    {
        pd3dImmediateContext->RSSetState( g_pRasterizerStateWireframe );
    }
    else
    {
        pd3dImmediateContext->RSSetState( g_pRasterizerStateSolid );
    }

    // Render the scene and optionally override the mesh topology and diffuse texture slot
    // Decide whether to use the user diffuse.dds
    UINT uDiffuseSlot = 0;
    if( ( g_eMeshType == MESH_TYPE_USER ) && ( NULL != g_pDiffuseTextureSRV ) )
    {
        uDiffuseSlot = INVALID_SAMPLER_SLOT;
    }
    // Decide which prim topology to use
    D3D11_PRIMITIVE_TOPOLOGY PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    if( bTessellation )
    {
        PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;
    }
    // Render the meshes    
    for( int iMesh = 0; iMesh < (int)g_SceneMesh[g_eMeshType].GetNumMeshes(); iMesh++ )
    {
        RenderMesh( &g_SceneMesh[g_eMeshType], (UINT)iMesh, PrimitiveTopology, uDiffuseSlot );
    }
    
    // Render GUI
    if( g_CmdLineParams.bRenderHUD )
    {
        DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );

        g_HUD.OnRender( fElapsedTime );
        g_SampleUI.OnRender( fElapsedTime );
        
        DXUT_EndPerfEvent();
    }

    // Always render text info 
    RenderText();

    // Decrement the exit frame counter
    g_CmdLineParams.iExitFrame--;

    // Exit on this frame
    if( g_CmdLineParams.iExitFrame == 0 )
    {
        if( g_CmdLineParams.bCapture )
        {
            CaptureFrame();
        }

        ::PostMessage( DXUTGetHWND(), WM_QUIT, 0, 0 );
    }
}


//--------------------------------------------------------------------------------------
// This callback function will be called immediately after the Direct3D device has 
// been destroyed, which generally happens as a result of application termination or 
// windowed/full screen toggles. Resources created in the OnD3D11CreateDevice callback 
// should be released here, which generally includes all D3DPOOL_MANAGED resources. 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11DestroyDevice();
    g_D3DSettingsDlg.OnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_DELETE( g_pTxtHelper );

    for( int iMeshType=0; iMeshType<MESH_TYPE_MAX; iMeshType++ )
    {
        g_SceneMesh[iMeshType].Destroy();
    }

    SAFE_RELEASE( g_pSceneVS );
    SAFE_RELEASE( g_pSceneWithTessellationVS );
    SAFE_RELEASE( g_pPNTrianglesHS );
    SAFE_RELEASE( g_pPNTrianglesAdaptiveHS );
    SAFE_RELEASE( g_pPNTrianglesDS );
    SAFE_RELEASE( g_pScenePS );
    SAFE_RELEASE( g_pTexturedScenePS );
        
    SAFE_RELEASE( g_pcbPNTriangles );

    SAFE_RELEASE( g_pCaptureTexture );

    SAFE_RELEASE( g_pSceneVertexLayout );
    
    SAFE_RELEASE( g_pSamplePoint );
    SAFE_RELEASE( g_pSampleLinear );

    SAFE_RELEASE( g_pRasterizerStateWireframe );
    SAFE_RELEASE( g_pRasterizerStateSolid );

    SAFE_RELEASE( g_pDiffuseTextureSRV );
}


//--------------------------------------------------------------------------------------
// Release swap chain
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11ReleasingSwapChain();

    SAFE_RELEASE( g_pCaptureTexture );
}


//--------------------------------------------------------------------------------------
// Find and compile the specified shader
//--------------------------------------------------------------------------------------
HRESULT CompileShaderFromFile( WCHAR* szFileName, D3D10_SHADER_MACRO* pDefines, LPCSTR szEntryPoint, 
                               LPCSTR szShaderModel, ID3DBlob** ppBlobOut )
{
    HRESULT hr = S_OK;

    // find the file
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, szFileName ) );

    DWORD dwShaderFlags = D3D10_SHADER_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3D10_SHADER_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3D10_SHADER_DEBUG;
#endif

    ID3DBlob* pErrorBlob;
    hr = D3DX11CompileFromFile( str, pDefines, NULL, szEntryPoint, szShaderModel, 
        dwShaderFlags, 0, NULL, ppBlobOut, &pErrorBlob, NULL );
    if( FAILED(hr) )
    {
        OutputDebugStringA( (char*)pErrorBlob->GetBufferPointer() );
        SAFE_RELEASE( pErrorBlob );
        return hr;
    }
    SAFE_RELEASE( pErrorBlob );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Helper function for command line retrieval
//--------------------------------------------------------------------------------------
bool IsNextArg( WCHAR*& strCmdLine, WCHAR* strArg )
{
   int nArgLen = (int) wcslen(strArg);
   int nCmdLen = (int) wcslen(strCmdLine);

   if( nCmdLen >= nArgLen && 
      _wcsnicmp( strCmdLine, strArg, nArgLen ) == 0 && 
      (strCmdLine[nArgLen] == 0 || strCmdLine[nArgLen] == L':' || strCmdLine[nArgLen] == L'=' ) )
   {
      strCmdLine += nArgLen;
      return true;
   }

   return false;
}


//--------------------------------------------------------------------------------------
// Helper function for command line retrieval.  Updates strCmdLine and strFlag 
//      Example: if strCmdLine=="-width:1024 -forceref"
// then after: strCmdLine==" -forceref" and strFlag=="1024"
//--------------------------------------------------------------------------------------
bool GetCmdParam( WCHAR*& strCmdLine, WCHAR* strFlag )
{
   if( *strCmdLine == L':' || *strCmdLine == L'=' )
   {       
      strCmdLine++; // Skip ':'

      // Place NULL terminator in strFlag after current token
      wcscpy_s( strFlag, 256, strCmdLine );
      WCHAR* strSpace = strFlag;
      while (*strSpace && (*strSpace > L' '))
         strSpace++;
      *strSpace = 0;

      // Update strCmdLine
      strCmdLine += wcslen(strFlag);
      return true;
   }
   else
   {
      strFlag[0] = 0;
      return false;
   }
}


//--------------------------------------------------------------------------------------
// Helper function to parse the command line
//--------------------------------------------------------------------------------------
void ParseCommandLine()
{
    // set some defaults
    g_CmdLineParams.DriverType = D3D_DRIVER_TYPE_HARDWARE;
    g_CmdLineParams.uWidth = 1024;
    g_CmdLineParams.uHeight = 768;
    g_CmdLineParams.bCapture = false;
    swprintf_s( g_CmdLineParams.strCaptureFilename, L"FrameCapture.bmp" );
    g_CmdLineParams.iExitFrame = -1;
    g_CmdLineParams.bRenderHUD = true;

    // Perform application-dependant command line processing
    WCHAR* strCmdLine = GetCommandLine();
    WCHAR strFlag[MAX_PATH];
    int nNumArgs;
    WCHAR** pstrArgList = CommandLineToArgvW( strCmdLine, &nNumArgs );
    for( int iArg=1; iArg<nNumArgs; iArg++ )
    {
        strCmdLine = pstrArgList[iArg];

        // Handle flag args
        if( *strCmdLine == L'/' || *strCmdLine == L'-' )
        {
            strCmdLine++;

            if( IsNextArg( strCmdLine, L"device" ) )
            {
                if( GetCmdParam( strCmdLine, strFlag ) )
                {
                    if ( (wcscmp(strFlag, L"HAL")==0) || (wcscmp(strFlag, L"TNLHAL")==0) )
                    {
                        g_CmdLineParams.DriverType = D3D_DRIVER_TYPE_HARDWARE;
                    }
                    else
                    {
                        g_CmdLineParams.DriverType = D3D_DRIVER_TYPE_REFERENCE;
                    }
                    continue;
                }
            }

            if( IsNextArg( strCmdLine, L"width" ) )
            {
                if( GetCmdParam( strCmdLine, strFlag ) )
                {
                   g_CmdLineParams.uWidth = _wtoi(strFlag);
                }
                continue;
            }

            if( IsNextArg( strCmdLine, L"height" ) )
            {
                if( GetCmdParam( strCmdLine, strFlag ) )
                {
                   g_CmdLineParams.uHeight = _wtoi(strFlag);
                }
                continue;
            }

            if( IsNextArg( strCmdLine, L"capturefilename" ) )
            {
                if( GetCmdParam( strCmdLine, strFlag ) )
                {
                   swprintf_s( g_CmdLineParams.strCaptureFilename, L"%s", strFlag );
                   g_CmdLineParams.bCapture = true;
                }
                continue;
            }

            if( IsNextArg( strCmdLine, L"nogui" ) )
            {
                g_CmdLineParams.bRenderHUD = false;
                continue;
            }

            if( IsNextArg( strCmdLine, L"exitframe" ) )
            {
                if( GetCmdParam( strCmdLine, strFlag ) )
                {
                   g_CmdLineParams.iExitFrame = _wtoi(strFlag);
                }
                continue;
            }
        }
    }
}


//--------------------------------------------------------------------------------------
// Utility function for surface creation
//--------------------------------------------------------------------------------------
HRESULT CreateSurface( ID3D11Texture2D** ppTexture, ID3D11ShaderResourceView** ppTextureSRV,
                       ID3D11RenderTargetView** ppTextureRTV, DXGI_FORMAT Format, unsigned int uWidth,
                       unsigned int uHeight )
{
    HRESULT hr;
    D3D11_TEXTURE2D_DESC Desc;
    D3D11_SHADER_RESOURCE_VIEW_DESC SRDesc;
    D3D11_RENDER_TARGET_VIEW_DESC RTDesc;

    SAFE_RELEASE( *ppTexture );
    SAFE_RELEASE( *ppTextureSRV );
    SAFE_RELEASE( *ppTextureRTV );

    ZeroMemory( &Desc, sizeof( Desc ) );
    Desc.Width = uWidth;
    Desc.Height = uHeight;
    Desc.MipLevels = 1;
    Desc.ArraySize = 1;
    Desc.Format = Format;
    Desc.SampleDesc.Count = 1;
    Desc.Usage = D3D11_USAGE_DEFAULT;
    Desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    hr = DXUTGetD3D11Device()->CreateTexture2D( &Desc, NULL, ppTexture );
    assert( D3D_OK == hr );

    SRDesc.Format = Format;
    SRDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    SRDesc.Texture2D.MostDetailedMip = 0;
    SRDesc.Texture2D.MipLevels = 1;
    hr = DXUTGetD3D11Device()->CreateShaderResourceView( *ppTexture, &SRDesc, ppTextureSRV );
    assert( D3D_OK == hr );

    RTDesc.Format = Format;
    RTDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    RTDesc.Texture2D.MipSlice = 0;
    hr = DXUTGetD3D11Device()->CreateRenderTargetView( *ppTexture, &RTDesc, ppTextureRTV );
    assert( D3D_OK == hr );

    return hr;
}


//--------------------------------------------------------------------------------------
// Helper function to capture a frame and dump it to disk 
//--------------------------------------------------------------------------------------
void CaptureFrame()
{
    // Retrieve RT resource
    ID3D11Resource *pRTResource;
    DXUTGetD3D11RenderTargetView()->GetResource(&pRTResource);

    // Retrieve a Texture2D interface from resource
    ID3D11Texture2D* RTTexture;
    pRTResource->QueryInterface( __uuidof( ID3D11Texture2D ), ( LPVOID* )&RTTexture);

    // Check if RT is multisampled or not
    D3D11_TEXTURE2D_DESC    TexDesc;
    RTTexture->GetDesc(&TexDesc);
    if (TexDesc.SampleDesc.Count>1)
    {
        // RT is multisampled, need resolving before dumping to disk

        // Create single-sample RT of the same type and dimensions
        DXGI_SAMPLE_DESC SingleSample = { 1, 0 };
        TexDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        TexDesc.MipLevels = 1;
        TexDesc.Usage = D3D11_USAGE_DEFAULT;
        TexDesc.CPUAccessFlags = 0;
        TexDesc.BindFlags = 0;
        TexDesc.SampleDesc = SingleSample;

        ID3D11Texture2D *pSingleSampleTexture;
        DXUTGetD3D11Device()->CreateTexture2D(&TexDesc, NULL, &pSingleSampleTexture );

        DXUTGetD3D11DeviceContext()->ResolveSubresource(pSingleSampleTexture, 0, RTTexture, 0, TexDesc.Format );

        // Copy RT into STAGING texture
        DXUTGetD3D11DeviceContext()->CopyResource(g_pCaptureTexture, pSingleSampleTexture);

        D3DX11SaveTextureToFile(DXUTGetD3D11DeviceContext(), g_pCaptureTexture, D3DX11_IFF_BMP, g_CmdLineParams.strCaptureFilename );

        SAFE_RELEASE(pSingleSampleTexture);
        
    }
    else
    {
        // Single sample case

        // Copy RT into STAGING texture
        DXUTGetD3D11DeviceContext()->CopyResource(g_pCaptureTexture, pRTResource);

        D3DX11SaveTextureToFile(DXUTGetD3D11DeviceContext(), g_pCaptureTexture, D3DX11_IFF_BMP, g_CmdLineParams.strCaptureFilename );
    }

    SAFE_RELEASE(RTTexture);

    SAFE_RELEASE(pRTResource);
}


//--------------------------------------------------------------------------------------
// Helper function that allows the app to render individual meshes of an sdkmesh
// and override the primitive topology
//--------------------------------------------------------------------------------------
void RenderMesh( CDXUTSDKMesh* pDXUTMesh, UINT uMesh, D3D11_PRIMITIVE_TOPOLOGY PrimType, 
                UINT uDiffuseSlot, UINT uNormalSlot, UINT uSpecularSlot )
{
    #define MAX_D3D11_VERTEX_STREAMS D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT

    assert( NULL != pDXUTMesh );

    if( 0 < pDXUTMesh->GetOutstandingBufferResources() )
    {
        return;
    }

    SDKMESH_MESH* pMesh = pDXUTMesh->GetMesh( uMesh );

    UINT Strides[MAX_D3D11_VERTEX_STREAMS];
    UINT Offsets[MAX_D3D11_VERTEX_STREAMS];
    ID3D11Buffer* pVB[MAX_D3D11_VERTEX_STREAMS];

    if( pMesh->NumVertexBuffers > MAX_D3D11_VERTEX_STREAMS )
    {
        return;
    }

    for( UINT64 i = 0; i < pMesh->NumVertexBuffers; i++ )
    {
        pVB[i] = pDXUTMesh->GetVB11( uMesh, (UINT)i );
        Strides[i] = pDXUTMesh->GetVertexStride( uMesh, (UINT)i );
        Offsets[i] = 0;
    }

    ID3D11Buffer* pIB;
    pIB = pDXUTMesh->GetIB11( uMesh );
    DXGI_FORMAT ibFormat = pDXUTMesh->GetIBFormat11( uMesh );
    
    DXUTGetD3D11DeviceContext()->IASetVertexBuffers( 0, pMesh->NumVertexBuffers, pVB, Strides, Offsets );
    DXUTGetD3D11DeviceContext()->IASetIndexBuffer( pIB, ibFormat, 0 );

    SDKMESH_SUBSET* pSubset = NULL;
    SDKMESH_MATERIAL* pMat = NULL;

    for( UINT uSubset = 0; uSubset < pMesh->NumSubsets; uSubset++ )
    {
        pSubset = pDXUTMesh->GetSubset( uMesh, uSubset );

        if( D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED == PrimType )
        {
            PrimType = pDXUTMesh->GetPrimitiveType11( ( SDKMESH_PRIMITIVE_TYPE )pSubset->PrimitiveType );
        }
        
        DXUTGetD3D11DeviceContext()->IASetPrimitiveTopology( PrimType );

        pMat = pDXUTMesh->GetMaterial( pSubset->MaterialID );
        if( uDiffuseSlot != INVALID_SAMPLER_SLOT && !IsErrorResource( pMat->pDiffuseRV11 ) )
        {
            DXUTGetD3D11DeviceContext()->PSSetShaderResources( uDiffuseSlot, 1, &pMat->pDiffuseRV11 );
        }

        if( uNormalSlot != INVALID_SAMPLER_SLOT && !IsErrorResource( pMat->pNormalRV11 ) )
        {
            DXUTGetD3D11DeviceContext()->PSSetShaderResources( uNormalSlot, 1, &pMat->pNormalRV11 );
        }

        if( uSpecularSlot != INVALID_SAMPLER_SLOT && !IsErrorResource( pMat->pSpecularRV11 ) )
        {
            DXUTGetD3D11DeviceContext()->PSSetShaderResources( uSpecularSlot, 1, &pMat->pSpecularRV11 );
        }

        UINT IndexCount = ( UINT )pSubset->IndexCount;
        UINT IndexStart = ( UINT )pSubset->IndexStart;
        UINT VertexStart = ( UINT )pSubset->VertexStart;
        
        DXUTGetD3D11DeviceContext()->DrawIndexed( IndexCount, IndexStart, VertexStart );
    }
}


//--------------------------------------------------------------------------------------
// Helper function to check for file existance
//--------------------------------------------------------------------------------------
bool FileExists( WCHAR* pFileName )
{
    DWORD fileAttr;    
    fileAttr = GetFileAttributes(pFileName);    
    if (0xFFFFFFFF == fileAttr)        
        return false;    
    return true;
}


//--------------------------------------------------------------------------------------
// EOF
//--------------------------------------------------------------------------------------
