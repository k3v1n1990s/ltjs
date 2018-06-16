
#include "precompile.h"

#ifdef LTJS_WIP_OGL
#include <cassert>
#include <algorithm>
#include <iterator>
#include <sstream>
#include <string>
#include <unordered_set>
#include "glad.h"
#include "ltjs_ogl_renderer.h"
#endif // LTJS_WIP_OGL

#include "3d_ops.h"
#include "common_init.h"
#include "tagnodes.h"
#include "renderinfostruct.h"

#include "d3d_shell.h"
#include "d3d_device.h"
#include "d3d_texture.h"
#include "d3d_draw.h"
#include "drawobjects.h"
#include "render.h"
#include "rendershadowlist.h"
#include "objectgroupmgr.h"
#include "screenglowmgr.h"
#include "rendererframestats.h"

// Shadow texture map related includes...
#include "..\shadows\d3dshadowtexture.h"


RMode* g_pModeList;
RMode* rdll_GetSupportedModes();
void rdll_FreeModeList(RMode *pModes);
void rdll_RenderDLLSetup(RenderStruct *pStruct);


// ---------------------------------------------------------------- //
// RenderStruct functions in other modules..
// ---------------------------------------------------------------- //
bool d3d_StartOptimized2D();
void d3d_EndOptimized2D();
bool d3d_SetOptimized2DBlend(LTSurfaceBlend blend);
bool d3d_GetOptimized2DBlend(LTSurfaceBlend &blend);
bool d3d_SetOptimized2DColor(HLTCOLOR color);
bool d3d_GetOptimized2DColor(HLTCOLOR &color);
bool d3d_IsInOptimized2D();
int  d3d_RenderScene(SceneDesc *pScene);
void d3d_SwapBuffers(uint flags);
bool d3d_GetScreenFormat(PFormat *pFormat);
bool d3d_OptimizeSurface(HLTBUFFER hBuffer, uint32 transparentColor);
void d3d_UnoptimizeSurface(HLTBUFFER hBuffer);
bool d3d_PostInitializeDevice(RenderStructInit *pInit,bool bFullInit);
HRENDERCONTEXT d3d_CreateContext();
void d3d_DeleteContext(HRENDERCONTEXT hContext);
void d3d_RenderCommand(int argc, char **argv);
bool d3d_AddGlowRenderStyleMapping(const char*, const char*);
bool d3d_SetGlowDefaultRenderStyle(const char*);
bool d3d_SetNoGlowRenderStyle(const char*);


HLTBUFFER d3d_CreateSurface(int width, int height);
void d3d_DeleteSurface(HLTBUFFER hSurf);
void d3d_GetSurfaceInfo(HLTBUFFER hSurf, uint32 *pWidth, uint32 *pHeight);
void* d3d_LockSurface(HLTBUFFER hSurf, uint32& Pitch);
void d3d_UnlockSurface(HLTBUFFER hSurf);
bool d3d_LockScreen(int left, int top, int right, int bottom, void **pData, long *pPitch);
void d3d_UnlockScreen();
void d3d_BlitToScreen(BlitRequest *pRequest);
void d3d_BlitFromScreen(BlitRequest *pRequest);
bool d3d_WarpToScreen(BlitRequest *pRequest);
void d3d_MakeScreenShot(const char *pFilename);
void d3d_MakeCubicEnvMap(const char* pszPrefix, uint32 nSize, const SceneDesc& InSceneDesc);
LPDIRECT3DDEVICE9 d3d_GetD3DDevice();
bool d3d_SetLightGroupColor(uint32 nID, const LTVector &vColor);
LTRESULT d3d_SetOccluderEnabled(uint32 nID, bool bEnabled);
LTRESULT d3d_GetOccluderEnabled(uint32 nID, bool *pEnabled);
uint32 d3d_GetTextureEffectVarID(const char* pszName, uint32 nStage);
bool   d3d_SetTextureEffectVar(uint32 nVarID, uint32 nVar, float fVar);


#ifdef LTJS_WIP_OGL
extern HWND ogl_window_;


namespace
{


// WGL_ARB_pixel_format
//
constexpr int WGL_ACCELERATION_ARB = 0x2003;
constexpr int WGL_ACCUM_ALPHA_BITS_ARB = 0x2021;
constexpr int WGL_ACCUM_BITS_ARB = 0x201D;
constexpr int WGL_ACCUM_BLUE_BITS_ARB = 0x2020;
constexpr int WGL_ACCUM_GREEN_BITS_ARB = 0x201F;
constexpr int WGL_ACCUM_RED_BITS_ARB = 0x201E;
constexpr int WGL_ALPHA_BITS_ARB = 0x201B;
constexpr int WGL_ALPHA_SHIFT_ARB = 0x201C;
constexpr int WGL_AUX_BUFFERS_ARB = 0x2024;
constexpr int WGL_BLUE_BITS_ARB = 0x2019;
constexpr int WGL_BLUE_SHIFT_ARB = 0x201A;
constexpr int WGL_COLOR_BITS_ARB = 0x2014;
constexpr int WGL_DEPTH_BITS_ARB = 0x2022;
constexpr int WGL_DOUBLE_BUFFER_ARB = 0x2011;
constexpr int WGL_DRAW_TO_BITMAP_ARB = 0x2002;
constexpr int WGL_DRAW_TO_WINDOW_ARB = 0x2001;
constexpr int WGL_FULL_ACCELERATION_ARB = 0x2027;
constexpr int WGL_GENERIC_ACCELERATION_ARB = 0x2026;
constexpr int WGL_GREEN_BITS_ARB = 0x2017;
constexpr int WGL_GREEN_SHIFT_ARB = 0x2018;
constexpr int WGL_NEED_PALETTE_ARB = 0x2004;
constexpr int WGL_NEED_SYSTEM_PALETTE_ARB = 0x2005;
constexpr int WGL_NO_ACCELERATION_ARB = 0x2025;
constexpr int WGL_NUMBER_OVERLAYS_ARB = 0x2008;
constexpr int WGL_NUMBER_PIXEL_FORMATS_ARB = 0x2000;
constexpr int WGL_NUMBER_UNDERLAYS_ARB = 0x2009;
constexpr int WGL_PIXEL_TYPE_ARB = 0x2013;
constexpr int WGL_RED_BITS_ARB = 0x2015;
constexpr int WGL_RED_SHIFT_ARB = 0x2016;
constexpr int WGL_SHARE_ACCUM_ARB = 0x200E;
constexpr int WGL_SHARE_DEPTH_ARB = 0x200C;
constexpr int WGL_SHARE_STENCIL_ARB = 0x200D;
constexpr int WGL_STENCIL_BITS_ARB = 0x2023;
constexpr int WGL_STEREO_ARB = 0x2012;
constexpr int WGL_SUPPORT_GDI_ARB = 0x200F;
constexpr int WGL_SUPPORT_OPENGL_ARB = 0x2010;
constexpr int WGL_SWAP_COPY_ARB = 0x2029;
constexpr int WGL_SWAP_EXCHANGE_ARB = 0x2028;
constexpr int WGL_SWAP_LAYER_BUFFERS_ARB = 0x2006;
constexpr int WGL_SWAP_METHOD_ARB = 0x2007;
constexpr int WGL_SWAP_UNDEFINED_ARB = 0x202A;
constexpr int WGL_TRANSPARENT_ALPHA_VALUE_ARB = 0x203A;
constexpr int WGL_TRANSPARENT_ARB = 0x200A;
constexpr int WGL_TRANSPARENT_BLUE_VALUE_ARB = 0x2039;
constexpr int WGL_TRANSPARENT_GREEN_VALUE_ARB = 0x2038;
constexpr int WGL_TRANSPARENT_INDEX_VALUE_ARB = 0x203B;
constexpr int WGL_TRANSPARENT_RED_VALUE_ARB = 0x2037;
constexpr int WGL_TYPE_COLORINDEX_ARB = 0x202C;
constexpr int WGL_TYPE_RGBA_ARB = 0x202B;

// WGL_ARB_create_context, WGL_ARB_create_context_profile
//
constexpr int WGL_CONTEXT_DEBUG_BIT_ARB = 0x0001;
constexpr int WGL_CONTEXT_FLAGS_ARB = 0x2094;
constexpr int WGL_CONTEXT_LAYER_PLANE_ARB = 0x2093;
constexpr int WGL_CONTEXT_MAJOR_VERSION_ARB = 0x2091;
constexpr int WGL_CONTEXT_MINOR_VERSION_ARB = 0x2092;
constexpr int WGL_CONTEXT_PROFILE_MASK_ARB = 0x9126;

constexpr int WGL_CONTEXT_CORE_PROFILE_BIT_ARB = 0x00000001;
constexpr int WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB = 0x00000002;
constexpr int WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB = 0x0002;

constexpr DWORD ERROR_INVALID_PROFILE_ARB = 0x2096;
constexpr DWORD ERROR_INVALID_VERSION_ARB = 0x2095;


using ExtensionSet = std::unordered_set<std::string>;


using WglGetExtensionsStringArbFunc = const char* (WINAPI *)(
	HDC hdc);

using WglChoosePixelFormatArbFunc = BOOL (WINAPI *)(
	HDC hdc,
	const int* piAttribIList,
	const FLOAT* pfAttribFList,
	UINT nMaxFormats,
	int* piFormats,
	UINT* nNumFormats);

using WglCreateContextAttribsArbFunc = HGLRC (WINAPI *)(
	HDC hDC,
	HGLRC hshareContext,
	const int* attribList);


constexpr auto ogl_color_bit_depth = 32;
constexpr auto ogl_depth_buffer_bit_depth = 24;
constexpr auto ogl_stencil_buffer_bit_depth = 8;
constexpr auto ogl_aux_buffer_count = 0;
constexpr auto ogl_core_major = 3;
constexpr auto ogl_core_minor = 3;


bool ogl_is_initialized_ = false;

HDC ogl_window_dc_ = nullptr;

int ogl_pixel_format_index_ = 0;
ExtensionSet ogl_wgl_extensions_;
WglChoosePixelFormatArbFunc wglChoosePixelFormatARB = nullptr;
WglCreateContextAttribsArbFunc wglCreateContextAttribsARB = nullptr;

GLuint ogl_test_vbo_ = 0;
GLuint ogl_test_vao_ = 0;


bool ogl_is_succeed()
{
	while (::glGetError() != GL_NO_ERROR)
	{
		return false;
	}

	return true;
}

void ogl_uninitialize()
{
	ogl_is_initialized_ = false;
	ogl_window_dc_ = nullptr;

	if (ogl_test_vao_)
	{
		::glDeleteVertexArrays(1, &ogl_test_vao_);
		ogl_test_vao_ = 0;
	}

	if (ogl_test_vbo_)
	{
		::glDeleteBuffers(1, &ogl_test_vbo_);
		ogl_test_vbo_ = 0;
	}

	ltjs::OglRenderer::get_instance().uninitialize();

	auto ogl_context = ::wglGetCurrentContext();
	auto ogl_dc = ::wglGetCurrentDC();

	if (ogl_dc)
	{
		static_cast<void>(::wglMakeCurrent(nullptr, nullptr));
	}

	if (ogl_context)
	{
		static_cast<void>(::wglDeleteContext(ogl_context));
	}

	ogl_pixel_format_index_ = 0;
	ogl_wgl_extensions_.clear();
	wglChoosePixelFormatARB = nullptr;
	wglCreateContextAttribsARB = nullptr;
}

void ogl_parse_wgl_extensions_string(
	const char* const wgl_extensions_string)
{
	ogl_wgl_extensions_.clear();

	if (!wgl_extensions_string)
	{
		return;
	}

	auto iss = std::istringstream{wgl_extensions_string};

	ogl_wgl_extensions_ = ExtensionSet{
		std::istream_iterator<std::string>{iss},
		std::istream_iterator<std::string>{}
	};
}

bool ogl_has_wgl_extension(
	const char* const extension_name)
{
	if (!extension_name)
	{
		return false;
	}

	return ogl_wgl_extensions_.find(extension_name) != ogl_wgl_extensions_.cend();
}

int ogl_choose_pixel_format_arb(
	HDC dc)
{
	static const int attributes[] =
	{
		WGL_DRAW_TO_WINDOW_ARB, 1,
		WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
		WGL_SUPPORT_OPENGL_ARB, 1,
		WGL_DOUBLE_BUFFER_ARB, 1,
		WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
		WGL_COLOR_BITS_ARB, ogl_color_bit_depth,
		WGL_DEPTH_BITS_ARB, ogl_depth_buffer_bit_depth,
		WGL_STENCIL_BITS_ARB, ogl_stencil_buffer_bit_depth,
		WGL_AUX_BUFFERS_ARB, ogl_aux_buffer_count,
		0, 0,
	}; // attributes


	auto pixel_format = 0;
	auto pixel_format_count = UINT{};

	const auto wgl_result = wglChoosePixelFormatARB(
		dc,
		attributes,
		nullptr,
		1,
		&pixel_format,
		&pixel_format_count);

	if (!wgl_result || pixel_format_count == 0)
	{
		return 0;
	}

	return pixel_format;
}

void ogl_detect_wgl_extensions()
{
	const auto dummy_class_name = L"ltjs_dummy";
	constexpr auto dummy_window_width = 64;
	constexpr auto dummy_window_height = 64;

	auto instance = ::GetModuleHandleW(nullptr);

	if (instance)
	{
		WNDCLASSW dummy_window_class;
		ZeroMemory(&dummy_window_class, sizeof(WNDCLASSW));
		dummy_window_class.style = CS_OWNDC;
		dummy_window_class.lpfnWndProc = DefWindowProcW;
		dummy_window_class.hInstance = instance;
		dummy_window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BACKGROUND);
		dummy_window_class.lpszClassName = dummy_class_name;

		const auto window_class_atom = ::RegisterClassW(&dummy_window_class);

		if (window_class_atom > 0)
		{
			auto dummy_window = CreateWindowExW(
				0, // extra style
				dummy_class_name, // window class name
				L"dummy", // window caption
				WS_CAPTION, // window style
				0, // initial x position
				0, // initial y position
				dummy_window_width, // initial x size
				dummy_window_height, // initial y size
				nullptr, // parent window handle
				nullptr, // window menu handle
				instance, // program instance handle
				nullptr // creation parameters
			);

			PIXELFORMATDESCRIPTOR pfd;
			ZeroMemory(&pfd, sizeof(PIXELFORMATDESCRIPTOR));
			pfd.nSize = static_cast<WORD>(sizeof(PIXELFORMATDESCRIPTOR));
			pfd.nVersion = 1;
			pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
			pfd.iPixelType = PFD_TYPE_RGBA;
			pfd.cColorBits = ogl_color_bit_depth;
			pfd.cDepthBits = ogl_depth_buffer_bit_depth;
			pfd.cStencilBits = ogl_stencil_buffer_bit_depth;
			pfd.cAuxBuffers = ogl_aux_buffer_count;

			const auto dc = ::GetDC(dummy_window);
			const auto legacy_pixel_format_index = ::ChoosePixelFormat(dc, &pfd);

			if (legacy_pixel_format_index > 0)
			{
				if (::SetPixelFormat(dc, legacy_pixel_format_index, &pfd))
				{
					ogl_pixel_format_index_ = legacy_pixel_format_index;

					const auto glrc = ::wglCreateContext(dc);

					if (::wglMakeCurrent(dc, glrc))
					{
						auto wglGetExtensionsStringARB = reinterpret_cast<WglGetExtensionsStringArbFunc>(
							::wglGetProcAddress("wglGetExtensionsStringARB"));

						if (wglGetExtensionsStringARB)
						{
							const auto extensions_string = wglGetExtensionsStringARB(dc);

							ogl_parse_wgl_extensions_string(extensions_string);

							if (ogl_has_wgl_extension("WGL_ARB_pixel_format"))
							{
								wglChoosePixelFormatARB = reinterpret_cast<WglChoosePixelFormatArbFunc>(
									::wglGetProcAddress("wglChoosePixelFormatARB"));
							}

							if (ogl_has_wgl_extension("WGL_ARB_create_context") &&
								ogl_has_wgl_extension("WGL_ARB_create_context_profile"))
							{
								wglCreateContextAttribsARB = reinterpret_cast<WglCreateContextAttribsArbFunc>(
									::wglGetProcAddress("wglCreateContextAttribsARB"));
							}
						}

						static_cast<void>(::wglMakeCurrent(nullptr, nullptr));
					}

					static_cast<void>(::wglDeleteContext(glrc));
				}
			}

			static_cast<void>(::DestroyWindow(dummy_window));

			static_cast<void>(::UnregisterClassW(
				reinterpret_cast<LPCWSTR>(static_cast<std::size_t>(window_class_atom)),
				instance));
		}
	}
}

bool ogl_set_pixel_format()
{
	int pixel_format_index = 0;

	if (wglChoosePixelFormatARB)
	{
		pixel_format_index = ogl_choose_pixel_format_arb(ogl_window_dc_);
	}

	if (pixel_format_index <= 0)
	{
		pixel_format_index = ogl_pixel_format_index_;
	}

	if (pixel_format_index <= 0)
	{
		return false;
	}

	PIXELFORMATDESCRIPTOR pfd;
	ZeroMemory(&pfd, sizeof(PIXELFORMATDESCRIPTOR));

	if (!::DescribePixelFormat(ogl_window_dc_, pixel_format_index, static_cast<UINT>(sizeof(PIXELFORMATDESCRIPTOR)), &pfd))
	{
		return false;
	}

	if (!::SetPixelFormat(ogl_window_dc_, pixel_format_index, &pfd))
	{
		return false;
	}

	return true;
}

bool ogl_create_context_and_make_current()
{
	if (!wglCreateContextAttribsARB)
	{
		return false;
	}

	static const int attributes[] =
	{
		WGL_CONTEXT_MAJOR_VERSION_ARB, ogl_core_major,
		WGL_CONTEXT_MINOR_VERSION_ARB, ogl_core_minor,
		WGL_CONTEXT_FLAGS_ARB, 0,
		WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
		0, 0,
	}; // attributes

	auto dc = ::GetDC(ogl_window_);
	auto glrc = wglCreateContextAttribsARB(dc, nullptr, attributes);

	if (!::wglMakeCurrent(dc, glrc))
	{
		return false;
	}

	return true;
}

void ogl_set_test_data()
{
	::glGenBuffers(1, &ogl_test_vbo_);
	::glGenVertexArrays(1, &ogl_test_vao_);


	static const GLfloat test_data[] =
	{
		-1.0F, -1.0F, 0.0F,
		0.0F, 1.0F, 0.0F,
		1.0F, -1.0F, 0.0F,
	}; // test_data

	::glBindVertexArray(ogl_test_vao_);

	::glBindBuffer(GL_ARRAY_BUFFER, ogl_test_vbo_);
	::glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizei>(sizeof(test_data)), test_data, GL_STATIC_DRAW);

	::glEnableVertexAttribArray(0);
	::glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<void*>(0));

	::glBindVertexArray(0);
}

bool ogl_initialize_internal(
	const int screen_width,
	const int screen_height)
{
	ogl_window_dc_ = ::GetDC(ogl_window_);

	ogl_detect_wgl_extensions();

	if (!ogl_set_pixel_format())
	{
		return false;
	}

	if (!ogl_create_context_and_make_current())
	{
		return false;
	}

	if (!::gladLoadGL())
	{
		return false;
	}

	auto& ogl_renderer = ltjs::OglRenderer::get_instance();

	if (!ogl_renderer.initialize(screen_width, screen_height))
	{
	}

	ogl_set_test_data();

	assert(ogl_is_succeed());

	ogl_renderer.set_current_context(false);

	return true;
}

void ogl_initialize(
	const int screen_width,
	const int screen_height)
{
	if (ogl_is_initialized_)
	{
		return;
	}

	if (!ogl_initialize_internal(screen_width, screen_height))
	{
		ogl_uninitialize();
	}

	ogl_is_initialized_ = true;
}


} // namespace


void ogl_swap_buffers()
{
	if (!ogl_is_initialized_)
	{
		return;
	}

	assert(ogl_is_succeed());
	ltjs::OglRenderer::get_instance().set_current_context(false);

	const auto swap_result = ::SwapBuffers(ogl_window_dc_);
	assert(swap_result);
}

void ogl_test_draw()
{
	::glBindVertexArray(ogl_test_vao_);
	assert(ogl_is_succeed());
	::glDrawArrays(GL_TRIANGLES, 0, 3);
	assert(ogl_is_succeed());
	::glBindVertexArray(0);
	assert(ogl_is_succeed());
}
#endif // LTJS_WIP_OGL


static bool d3d_IsNullRenderOn()
{
	return !!g_pStruct->GetParameterValueFloat(g_pStruct->GetParameter("nullrender"));
}

bool g_bInStandby = false;						// The renderer can be put into standby (by calling d3d_Term(false)) - then reinited (d3d_Init(...)) later on... (Mostly for ALT-Tab)
void d3d_Term(bool bFullTerm)						// We don't do a FullTerm on Alt-Tab (will be coming back up, basically - don't really release the device)...
{
	d3d_TermObjectModules();

	g_Device.ReleaseDevObjects(bFullTerm);		// Let the RenderObject release their D3D Data...

	g_TextureManager.Term(bFullTerm);			// Term the TextureManager...

	delete D3DShadowTextureFactory::Get( );		// Shadow texture map related termination stuff...
	CRenderShadowList::GetSingleton().InvalidateShadowTextures(); // make sure that future shadows will be regenerated

	if (bFullTerm) 
	{
		//we need to make sure that the render styles associated with the glow texture are released before
		//they are deleted by D3DShell
		CScreenGlowMgr::GetSingleton().GetRenderStyleMap().FreeList();

		g_Device.FreeAll(); 					// Free the Device...
		g_D3DShell.FreeAll();					// Free the Shell...

		g_bInStandby = false;

#ifdef LTJS_WIP_OGL
		ogl_uninitialize();
#endif // LTJS_WIP_OGL
	}
	else
	{
		g_bInStandby = g_Device.Standby();
	}

	ShowCursor(true);							// Show the cursor
}

int d3d_Init(RenderStructInit *pInit)
{
	char* pStr; RECT wndRect,screenRect;
	pInit->m_RendererVersion = LTRENDER_VERSION;

#ifdef _DEBUG
	pStr = g_pStruct->GetParameterValueString(g_pStruct->GetParameter("RenderDebugBreak"));
	if (pStr && atoi(pStr) == 1) { DebugBreak(); }
#endif

	d3d_CreateConsoleVariables();					// Read the initial console variables in.
	d3d_ReadConsoleVariables();

	// Run windowed?
	pStr = g_pStruct->GetParameterValueString(g_pStruct->GetParameter("windowed"));
	g_bRunWindowed = (pStr && atoi(pStr) == 1);

	// Init globals.
	g_hWnd				= (HWND)pInit->m_hWnd;
	g_ScreenWidth		= pInit->m_Mode.m_Width;
	g_ScreenHeight		= pInit->m_Mode.m_Height;

	D3DAdapterInfo* pAdapterInfo	 = NULL;
	D3DDeviceInfo*  pDeviceInfo		 = NULL;
	D3DModeInfo*	pModeInfo		 = NULL;

	// If we're in standby mode, try to bring the device back the easy way
	if (g_bInStandby) 
	{
		pAdapterInfo				  = g_Device.GetAdapterInfo();
		pDeviceInfo					  = g_Device.GetDeviceInfo();
		pModeInfo					  = g_D3DShell.PickDefaultMode(pDeviceInfo, pInit->m_Mode.m_Width, pInit->m_Mode.m_Height, 
																				pInit->m_Mode.m_BitDepth, pInit->m_Mode.m_bHWTnL);

		if (!pModeInfo)								
		{
			// Couldn't find any devices that make the cut - try without standby
			// Shut down the rest of the way if we failed...
			d3d_Term(true);
			pDeviceInfo = NULL;
			g_bInStandby = false;
		}
		else 
		{
			// Try to restore the display mode
			if (!g_Device.SetMode(pModeInfo))
			{
				// Shut down the renderer if we failed...
				g_Device.FreeDevice();
				pDeviceInfo = NULL;
				g_bInStandby = false;
			}
			else
				g_Device.SetDefaultRenderStates();		// Set the default render states...
		}
	}		

	if (!g_bInStandby) 
	{
		// Create the D3DShell (it'll enumerate all the hardware devices)...
		if (!g_D3DShell.Create()) 
			return RENDER_ERROR;

		// First try to find your favorite device. If you can't, pick a default one.
		uint32 iAdapterID			  = 0;			// D3D's adapter counter number for this adapter...
        static_cast<void>(iAdapterID);
		
		if (*pInit->m_Mode.m_InternalName) 
			g_D3DShell.FindDevice(pInit->m_Mode.m_InternalName,&pAdapterInfo);
		
		if (!pDeviceInfo) 
			pDeviceInfo = g_D3DShell.PickDefaultDev(&pAdapterInfo);

		if (!pDeviceInfo) 
		{	
			// Couldn't find any devices that make the cut - fail out
			d3d_Term(true); 
			AddDebugMessage(0, "Can't find any d3d devices to use!"); 
			return RENDER_ERROR; 
		}

		pModeInfo  = g_D3DShell.PickDefaultMode(pDeviceInfo, pInit->m_Mode.m_Width, pInit->m_Mode.m_Height, 
																					pInit->m_Mode.m_BitDepth, pInit->m_Mode.m_bHWTnL);

		if (!pModeInfo) 
		{	
			// Couldn't find any devices that make the cut - fail out
			d3d_Term(true); 
			AddDebugMessage(0, "Can't find an appropriate display mode!"); 
			return RENDER_ERROR; 
		}

		// Create the Device...
		if (!g_Device.CreateDevice(pAdapterInfo,pDeviceInfo,pModeInfo)) 
		{
			d3d_Term(true); 
			AddDebugMessage(0, "Couldn't create D3D Device!"); 
			return RENDER_ERROR; 
		}
	}

	// We're definitely not in standby mode any more
	g_bInStandby = false;

	ShowCursor(false);

	g_Device.RestoreDevObjects();					// Let the render objects restore their D3D data (if there is any already created)...

	// Copy the info for the device we used into the RMode.
	LTStrCpy(pInit->m_Mode.m_InternalName, pDeviceInfo->strDesc, sizeof(pInit->m_Mode.m_InternalName));
	LTStrCpy(pInit->m_Mode.m_Description,  pAdapterInfo->AdapterID.Description, sizeof(pInit->m_Mode.m_Description));

	// Finish initializing...
	AddDebugMessage(0, "Using Direct3D Device %s", pDeviceInfo->strDesc);

	// Either maximize the window or set its size.
	if (g_bRunWindowed || d3d_IsNullRenderOn()) 
	{
		GetWindowRect(GetDesktopWindow(), &screenRect);
		
		// Setup the client rectangle.
		wndRect.left		= ((screenRect.right - screenRect.left) - pInit->m_Mode.m_Width) / 2;
		wndRect.top			= ((screenRect.bottom - screenRect.top) - pInit->m_Mode.m_Height) / 2;
		wndRect.right		= wndRect.left + pInit->m_Mode.m_Width;
		wndRect.bottom		= wndRect.top + pInit->m_Mode.m_Height;

		// Figure out the full window coordinates given the client coordinates.
		AdjustWindowRect(&wndRect, GetWindowLong(g_hWnd,GWL_STYLE), false);

		if (wndRect.left < 0) 
		{
			wndRect.right	-= wndRect.left;
			wndRect.left	 = 0; 
		}
		if (wndRect.top < 0) 
		{
			wndRect.bottom	-= wndRect.top;
			wndRect.top		 = 0; 
		}

#ifdef LTJS_WIP_OGL
		wndRect.right -= wndRect.left;
		wndRect.left = 0;
#endif // LTJS_WIP_OGL

		SetWindowPos(g_hWnd, 0, wndRect.left, wndRect.top, wndRect.right-wndRect.left,
			wndRect.bottom-wndRect.top, SWP_NOREPOSITION); 
	}
	else 
	{
		SetWindowPos(g_hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE); 
	}

	if (!d3d_PostInitializeDevice(pInit,!g_bInStandby)) 
		return RENDER_ERROR;

	if (!d3d_GetVisibleSet()->Init()) 
	{
		d3d_Term(true);
		AddDebugMessage(0, "VisibleSet::Init failed (invalid object list size?).");
 
		return RENDER_ERROR; 
	} 

#ifdef LTJS_WIP_OGL
	ogl_initialize(
		static_cast<int>(pInit->m_Mode.m_Width),
		static_cast<int>(pInit->m_Mode.m_Height));
#endif // LTJS_WIP_OGL

	LT_MEM_TRACK_ALLOC(new D3DShadowTextureFactory(),LT_MEM_TYPE_RENDERER); // Shadow texture map related initialization stuff added here

	return RENDER_OK;
}

bool d3d_LoadWorldData(ILTStream *pStream)
{
	return g_Device.LoadWorldData(pStream);
}

void d3d_GetRenderInfo(RenderInfoStruct* pStruct)
{
	if (pStruct) 
	{
		pStruct->m_dwWorldPolysDrawn     = FrameStat(eFS_WorldTriangles);
		pStruct->m_dwWorldPolysProcessed = FrameStat(eFS_WorldTriangles);
		pStruct->m_dwModelPolysDrawn     = FrameStat(eFS_WorldTriangles); 
	}
}

// Note: if you want to use these in a launcher of some sort, you need to also add the 
//	CD3D_Shell class. See the Launcher sample.
RMode* rdll_GetSupportedModes() 
{
	// Make sure we've created our shell...
	if (!PDIRECT3D) 
		g_D3DShell.Create();

	if (!PDIRECT3D) 
		return NULL;

	g_pModeList = NULL;
	g_D3DShell.GetSupportedModes(g_pModeList);
	return g_pModeList;
}

void rdll_FreeModeList(RMode* pModes)
{
	RMode* pCur = pModes;
	while (pCur) 
	{
		RMode* pNext = pCur->m_pNext;
		delete pCur;
		pCur = pNext; 
	}
}

void rdll_RenderDLLSetup(RenderStruct *pStruct)
{
	g_pStruct = pStruct;

	pStruct->Init						= d3d_Init;
	pStruct->Term						= d3d_Term;
	pStruct->GetD3DDevice				= d3d_GetD3DDevice;
	pStruct->BindTexture				= d3d_BindTexture;
	pStruct->UnbindTexture				= d3d_UnbindTexture;
	pStruct->QueryDDSupport				= CTextureManager::QueryDDSupport;
	pStruct->GetTextureDDFormat1		= CTextureManager::QueryDDFormat1;
	pStruct->GetTextureDDFormat2		= CTextureManager::QueryDDFormat2;
	pStruct->ConvertTexDataToDD			= CTextureManager::ConvertTexDataToDD;
	pStruct->DrawPrimSetTexture			= CTextureManager::DrawPrimSetTexture;
	pStruct->DrawPrimDisableTextures	= CTextureManager::DisableTextures;
	pStruct->CreateContext				= d3d_CreateContext;
	pStruct->DeleteContext				= d3d_DeleteContext;
	pStruct->Clear						= d3d_Clear;
	pStruct->Start3D					= CD3D_Device::Start3D;
	pStruct->End3D						= CD3D_Device::End3D;
	pStruct->IsIn3D						= CD3D_Device::IsIn3D;
	pStruct->StartOptimized2D			= d3d_StartOptimized2D;
	pStruct->EndOptimized2D				= d3d_EndOptimized2D;
	pStruct->SetOptimized2DBlend		= d3d_SetOptimized2DBlend;
	pStruct->GetOptimized2DBlend		= d3d_GetOptimized2DBlend;
	pStruct->SetOptimized2DColor		= d3d_SetOptimized2DColor;
	pStruct->GetOptimized2DColor		= d3d_GetOptimized2DColor;
	pStruct->IsInOptimized2D			= d3d_IsInOptimized2D;
	pStruct->OptimizeSurface			= d3d_OptimizeSurface;
	pStruct->UnoptimizeSurface			= d3d_UnoptimizeSurface;
	pStruct->RenderScene				= d3d_RenderScene;
	pStruct->RenderCommand				= d3d_RenderCommand;
	pStruct->SwapBuffers				= d3d_SwapBuffers;
	pStruct->GetScreenFormat			= d3d_GetScreenFormat;
	pStruct->CreateSurface				= d3d_CreateSurface;
	pStruct->DeleteSurface				= d3d_DeleteSurface;
	pStruct->GetSurfaceInfo				= d3d_GetSurfaceInfo;
	pStruct->LockSurface				= d3d_LockSurface;
	pStruct->UnlockSurface				= d3d_UnlockSurface;
	pStruct->LockScreen					= d3d_LockScreen;
	pStruct->UnlockScreen				= d3d_UnlockScreen;
	pStruct->MakeScreenShot				= d3d_MakeScreenShot;
	pStruct->MakeCubicEnvMap			= d3d_MakeCubicEnvMap;
	pStruct->ReadConsoleVariables		= d3d_ReadConsoleVariables;
	pStruct->GetRenderInfo				= d3d_GetRenderInfo;
	pStruct->CreateRenderObject			= CD3D_Device::CreateRenderObject;
	pStruct->DestroyRenderObject		= CD3D_Device::DestroyRenderObject;
	pStruct->BlitToScreen				= d3d_BlitToScreen;
	pStruct->BlitFromScreen				= d3d_BlitFromScreen;
	pStruct->WarpToScreen				= d3d_WarpToScreen;
	pStruct->LoadWorldData				= d3d_LoadWorldData;
	pStruct->SetLightGroupColor			= d3d_SetLightGroupColor;
	pStruct->SetOccluderEnabled			= d3d_SetOccluderEnabled;
	pStruct->GetOccluderEnabled			= d3d_GetOccluderEnabled;
	pStruct->GetTextureEffectVarID		= d3d_GetTextureEffectVarID;
	pStruct->SetTextureEffectVar		= d3d_SetTextureEffectVar;
	pStruct->IsObjectGroupEnabled		= CObjectGroupMgr::IsObjectGroupEnabled;
	pStruct->SetObjectGroupEnabled		= CObjectGroupMgr::SetObjectGroupEnabled;
	pStruct->SetAllObjectGroupEnabled	= CObjectGroupMgr::SetAllObjectGroupEnabled;
	pStruct->AddGlowRenderStyleMapping	= d3d_AddGlowRenderStyleMapping;
	pStruct->SetGlowDefaultRenderStyle	= d3d_SetGlowDefaultRenderStyle;
	pStruct->SetNoGlowRenderStyle		= d3d_SetNoGlowRenderStyle;

	//initialize our object group list
	CObjectGroupMgr::InitObjectGroups();
}

