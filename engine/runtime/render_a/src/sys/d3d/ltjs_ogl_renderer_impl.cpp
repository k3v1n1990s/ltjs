#include "precompile.h"
#include "ltjs_ogl_renderer.h"
#include <cassert>
#include <algorithm>
#include <array>
#include <bitset>
#include <iterator>
#include <list>
#include <memory>
#include <numeric>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include "glad.h"
#include "bibendovsky_spul_scope_guard.h"
#include "bibendovsky_spul_un_value.h"


// --------------------------------------------------------------------------
// GL_CLIP_VOLUME_CLIPPING_HINT_EXT
//

#ifndef GL_CLIP_VOLUME_CLIPPING_HINT_EXT
#define GL_CLIP_VOLUME_CLIPPING_HINT_EXT (0x80F0)
#endif // !GL_CLIP_VOLUME_CLIPPING_HINT_EXT

//
// GL_CLIP_VOLUME_CLIPPING_HINT_EXT
// --------------------------------------------------------------------------


// --------------------------------------------------------------------------
// GL_ARB_clip_control
//

#ifndef GL_CLIP_ORIGIN
#define GL_CLIP_ORIGIN (0x935C)
#endif // !GL_CLIP_ORIGIN

#ifndef GL_CLIP_DEPTH_MODE
#define GL_CLIP_DEPTH_MODE (0x935D)
#endif // !GL_CLIP_DEPTH_MODE

#ifndef GL_NEGATIVE_ONE_TO_ONE
#define GL_NEGATIVE_ONE_TO_ONE (0x935E)
#endif // !GL_NEGATIVE_ONE_TO_ONE

#ifndef GL_ZERO_TO_ONE
#define GL_ZERO_TO_ONE (0x935F)
#endif // !GL_ZERO_TO_ONE

using PFNGLCLIPCONTROLPROC = void (APIENTRYP)(GLenum origin, GLenum depth);
static PFNGLCLIPCONTROLPROC glClipControl = nullptr;

//
// GL_ARB_clip_control
// --------------------------------------------------------------------------


// --------------------------------------------------------------------------
// GL_EXT_texture_compression_s3tc
//

#ifndef GL_COMPRESSED_RGB_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT (0x83F0)
#endif // !GL_COMPRESSED_RGB_S3TC_DXT1_EXT

#ifndef GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT (0x83F1)
#endif // !GL_COMPRESSED_RGBA_S3TC_DXT1_EXT

#ifndef GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT (0x83F2)
#endif // !GL_COMPRESSED_RGBA_S3TC_DXT3_EXT

#ifndef GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT (0x83F3)
#endif // !GL_COMPRESSED_RGBA_S3TC_DXT5_EXT

//
// GL_EXT_texture_compression_s3tc
// --------------------------------------------------------------------------


// --------------------------------------------------------------------------
// GL_EXT_texture_filter_anisotropic
//

#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT (0x84FE)
#endif // !GL_TEXTURE_MAX_ANISOTROPY_EXT

#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT (0x84FF)
#endif // !GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT

//
// GL_EXT_texture_filter_anisotropic
// --------------------------------------------------------------------------


namespace ltjs
{


namespace
{


constexpr std::uint32_t d3dfvf_xyz = 0x002;
constexpr std::uint32_t d3dfvf_xyzrhw = 0x004;

constexpr std::uint32_t d3dfvf_xyzb1 = 0x006;
constexpr std::uint32_t d3dfvf_xyzb2 = 0x008;
constexpr std::uint32_t d3dfvf_xyzb3 = 0x00A;

constexpr std::uint32_t d3dfvf_normal = 0x010;

constexpr std::uint32_t d3dfvf_diffuse = 0x040;

constexpr std::uint32_t d3dfvf_tex1 = 0x100;
constexpr std::uint32_t d3dfvf_tex2 = 0x200;
constexpr std::uint32_t d3dfvf_tex3 = 0x300;
constexpr std::uint32_t d3dfvf_tex4 = 0x400;

constexpr std::uint32_t d3dfvf_textureformat2 = 0; // Two floating point values
constexpr std::uint32_t d3dfvf_textureformat3 = 1; // Three floating point values
constexpr std::uint32_t d3dfvf_textureformat4 = 2; // Four floating point values


template<typename T>
T ogl_resolve_symbol(
	const char* const symbol_name)
{
	return reinterpret_cast<T>(::wglGetProcAddress(symbol_name));
}


constexpr std::uint32_t d3dfvf_texcoordsize2(
	const int index)
{
	return d3dfvf_textureformat2;
}

constexpr std::uint32_t d3dfvf_texcoordsize3(
	const int index)
{
	return d3dfvf_textureformat3 << ((index * 2) + 16);
}

constexpr std::uint32_t d3dfvf_texcoordsize4(
	const int index)
{
	return d3dfvf_textureformat4 << ((index * 2) + 16);
}

constexpr auto d3dfvf_valid_flags =
	d3dfvf_xyz |
	d3dfvf_xyzrhw |
	d3dfvf_xyzb1 |
	d3dfvf_xyzb2 |
	d3dfvf_xyzb3 |
	d3dfvf_normal |
	d3dfvf_diffuse |
	d3dfvf_tex1 |
	d3dfvf_tex2 |
	d3dfvf_tex4 |
	d3dfvf_texcoordsize3(0) |
	d3dfvf_texcoordsize3(1) |
	d3dfvf_texcoordsize4(1) |
	d3dfvf_texcoordsize4(2) |
	d3dfvf_texcoordsize4(3) |
	0;

constexpr auto invalid_ogl_enum = GLenum{0xFFFFFFFF};
constexpr auto invalid_ogl_format = GLint{-1};


GLenum usage_flags_to_ogl_usage(
	const OglRenderer::VertexArrayObject::UsageFlags usage_flags)
{
	if ((usage_flags & OglRenderer::VertexArrayObject::UsageFlags::is_dynamic) != 0)
	{
		if ((usage_flags & OglRenderer::VertexArrayObject::UsageFlags::is_readable) != 0)
		{
			return GL_DYNAMIC_COPY;
		}
		else
		{
			return GL_DYNAMIC_DRAW;
		}
	}
	else
	{
		if ((usage_flags & OglRenderer::VertexArrayObject::UsageFlags::is_readable) != 0)
		{
			return GL_STATIC_COPY;
		}
		else
		{
			return GL_STATIC_DRAW;
		}
	}
}

bool is_surface_format_valid(
	const OglRenderer::SurfaceFormat& surface_format)
{
	switch (surface_format)
	{
	case OglRenderer::SurfaceFormat::a4r4g4b4:
	case OglRenderer::SurfaceFormat::a8r8g8b8:
	case OglRenderer::SurfaceFormat::v8u8:
	case OglRenderer::SurfaceFormat::dxt1:
	case OglRenderer::SurfaceFormat::dxt3:
	case OglRenderer::SurfaceFormat::dxt5:
		return true;

	default:
		return false;
	}
}


} // namespace


// ==========================================================================
// OglRendererImpl
//

class OglRendererImpl final :
	public OglRenderer
{
public:
	OglRendererImpl()
		:
		is_initialized_{},
		is_current_context_{},
		ogl_dc_{},
		ogl_rc_{},
		extensions_{},
		has_gl_arb_clip_control_{},
		has_gl_ext_clip_volume_hint_{},
		has_gl_arb_texture_compression_{},
		has_gl_ext_texture_compression_s3tc_{},
		has_gl_ext_texture_filter_anisotropic_{},
		max_anisotropy_{},
		screen_width_{},
		screen_height_{},
		is_dirty_{},
		clear_color_r_{},
		clear_color_g_{},
		clear_color_b_{},
		clear_color_a_{},
		viewport_{},
		max_viewport_size_{},
		cull_mode_{},
		fill_mode_{},
		is_clipping_{},
		is_depth_enabled_{},
		is_depth_writable_{},
		depth_func_{},
		is_blending_enabled_{},
		src_blending_factor_{},
		dst_blending_factor_{},
		vertex_shader_{},
		fragment_shader_{},
		program_{},
		has_diffuse_{},
		u_has_diffuse_{},
		is_position_transformed_{},
		are_u_model_views_dirty_{},
		is_u_projection_dirty_{},
		world_matrices_{},
		view_matrix_{},
		projection_matrix_{},
		u_model_views_{},
		u_projection_{},
		current_vao_{},
		ui_vaos_{},
		vaos_{},
		current_2d_texture_{},
		current_cube_map_texture_{},
		textures_{},
		sampler_states_{},
		max_texture_lod_bias_{}
	{
	}

	~OglRendererImpl() override
	{
		assert(!is_initialized_);
	}


private:
	static constexpr auto min_anisotropy = 1.0F;

	static constexpr auto max_world_matrices = 4;

	static constexpr auto max_ui_vao_vertices = 128;
	static constexpr auto max_ui_vaos = 4;

	using UiVaoD3dFvfs = std::array<std::uint32_t, max_ui_vaos>;

	static constexpr auto ui_vao_d3d_fvfs = UiVaoD3dFvfs
	{
		d3dfvf_xyz | d3dfvf_diffuse,
		d3dfvf_xyz | d3dfvf_diffuse | d3dfvf_tex1,
		d3dfvf_xyzrhw | d3dfvf_diffuse,
		d3dfvf_xyzrhw | d3dfvf_diffuse | d3dfvf_tex1,
	};


	using Matrix4F = std::array<float, 16>;

	using WorldMatrix = Matrix4F;
	using WorldMatrices = std::array<WorldMatrix, max_world_matrices>;

	using ModelViewMatrixDirtyFlags = std::bitset<max_world_matrices>;

	using ViewMatrix = Matrix4F;
	using ModelViewMatrix = Matrix4F;
	using ProjectionMatrix = Matrix4F;

	using UModelViewMatrices = std::array<GLint, max_world_matrices>;

	struct UiVao
	{
		std::uint32_t d3d_fvf_;
		int vertex_offset_;
		VertexArrayObjectPtr vao_;

		UiVao()
			:
			d3d_fvf_{},
			vertex_offset_{},
			vao_{}
		{
		}
	}; // UiVao

	using UiVaoPtr = UiVao*;

	using UiVaos = std::array<UiVao, max_ui_vaos>;


	class VertexArrayObjectImpl final :
		public VertexArrayObject
	{
	public:
		VertexArrayObjectImpl(
			OglRendererImpl& ogl_renderer);

		~VertexArrayObjectImpl() override;


	private:
		static constexpr auto index_element_size = 2;


		OglRendererImpl& ogl_renderer_;

		bool is_initialized_;

		Fvf vertex_format_;
		UsageFlags vertex_usage_flags_;
		UsageFlags index_usage_flags_;
		int vertex_count_;
		int index_count_;

		GLuint ogl_vao_;
		GLuint ogl_vertex_buffer_;
		GLuint ogl_index_buffer_;


		bool do_initialize(
			const InitializeParam& param) override;

		bool initialize_internal(
			const InitializeParam& param);

		void do_uninitialize() override;

		void do_set_vertex_data(
			const int vertex_index,
			const int vertex_count,
			const void* const raw_data) override;

		void do_draw(
			const PrimitiveType primitive_type,
			const int index_base,
			const int vertex_base,
			const int primitive_count) override;

		void uninitialize_internal();
	}; // VertexArrayObjectImpl

	using VertexArrayObjectUPtr = std::unique_ptr<VertexArrayObjectImpl>;
	using VertexArrayObjectUList = std::list<VertexArrayObjectUPtr>;


	class SamplerStateImpl :
		public SamplerState
	{
	public:
		struct InitializeParam
		{
			int unit_index_;
			float max_lod_bias_;
			bool has_anisotropy_;
			float max_anisotropy_;


			InitializeParam();

			bool is_valid() const;
		}; // InitializeParam


		SamplerStateImpl();

		~SamplerStateImpl() override;


		bool initialize(
			const InitializeParam& param);

		void uninitialize();


	private:
		static constexpr auto default_addressing_mode_u = TextureAddressingMode::wrap;
		static constexpr auto default_addressing_mode_v = TextureAddressingMode::wrap;

		static constexpr auto default_mag_filter = TextureFilterType::point;
		static constexpr auto default_min_filter = TextureFilterType::point;
		static constexpr auto default_mip_filter = TextureFilterType::none;

		static constexpr auto default_lod_bias = 0.0F;

		static constexpr auto default_max_anisotropy = min_anisotropy;


		bool is_initialized_;

		int unit_index_;
		GLuint ogl_sampler_;

		TextureAddressingMode addressing_mode_u_;
		TextureAddressingMode addressing_mode_v_;

		TextureFilterType mag_filter_;
		TextureFilterType min_filter_;
		TextureFilterType mip_filter_;

		float lod_bias_;
		float max_lod_bias_;

		bool has_anisotropy_;
		float anisotropy_;
		float max_anisotropy_;


		// ========================================================================
		// API
		//

		TextureAddressingMode do_get_addressing_mode_u() const override;

		void do_set_addressing_mode_u(
			const TextureAddressingMode addressing_mode_u) override;


		TextureAddressingMode do_get_addressing_mode_v() const override;

		void do_set_addressing_mode_v(
			const TextureAddressingMode addressing_mode_v) override;


		TextureFilterType do_get_mag_filter() const override;

		void do_set_mag_filter(
			const TextureFilterType mag_filter) override;


		TextureFilterType do_get_min_filter() const override;

		void do_set_min_filter(
			const TextureFilterType min_filter) override;


		TextureFilterType do_get_mip_filter() const override;

		void do_set_mip_filter(
			const TextureFilterType mip_filter) override;


		float do_get_lod_bias() const override;

		void do_set_lod_bias(
			const float lod_bias) override;


		float do_get_anisotropy() const override;

		void do_set_anisotropy(
			const float anisotropy) override;

		//
		// API
		// ========================================================================


		void set_addressing_mode_u_internal();

		void set_addressing_mode_v_internal();

		void set_mag_filter_internal();

		void set_minmip_filter_internal();

		void set_min_filter_internal();

		void set_mip_filter_internal();

		void set_lod_bias_internal();

		void set_anisotropy_internal();

		void set_defaults();


		static GLenum get_ogl_addressing_mode(
			const TextureAddressingMode d3d_addressing_mode);

		static GLenum get_ogl_mag_filter(
			const TextureFilterType d3d_mag_filter);

		static GLenum get_ogl_min_filter(
			const TextureFilterType d3d_min_filter,
			const TextureFilterType d3d_mip_filter);
	}; // SamplerStateImpl

	using SamplerStates = std::array<SamplerStateImpl, max_samplers>;


	class TextureImpl :
		public Texture
	{
	public:
		TextureImpl(
			OglRendererImpl& ogl_renderer);

		~TextureImpl() override;


	private:
		using Buffer = std::vector<ul::UnValue<std::uint8_t>>;


		OglRendererImpl& ogl_renderer_;

		bool is_initialized_;

		GLuint ogl_texture_;

		Type type_;
		bool is_compressed_;
		SurfaceFormat surface_format_;

		int width_;
		int height_;

		int level_count_;
		int max_level_count_;

		Buffer buffer_;


		// ==================================================================
		// API
		//

		bool do_initialize(
			const InitializeParam& param) override;

		void do_uninitialize() override;


		bool do_upload_level(
			const UploadParam& param) override;


		Type do_get_type() const override;

		SurfaceFormat do_get_surface_format() const override;

		bool do_is_compressed() const override;

		int do_get_width() const override;

		int do_get_height() const override;

		int do_get_level_count() const override;

		int do_get_level_width(
			const int level) const override;

		int do_get_level_height(
			const int level) const override;

		//
		// API
		// ==================================================================


		bool initialize_internal(
			const InitializeParam& param);

		bool initialize_2d_internal();

		bool initialize_cube_map_internal();

		void uninitialize_internal();

		static int calculate_max_levels(
			const int width,
			const int height);

		int get_ogl_internal_pixel_format() const;

		void get_ogl_pixel_format_and_type(
			GLenum& format,
			GLenum& type) const;

		int calculate_compressed_image_size(
			const int width,
			const int height) const;

		bool upload_level_no_filter(
			const UploadParam& param,
			const void* raw_data);

		bool upload_level_a4r4g4b4_linear(
			const UploadParam& param);

		bool upload_level_a8r8g8b8_to_v8u8_linear(
			const UploadParam& param);

		static std::uint16_t interpolate_pixel_linearly_a4r4g4b4(
			const int x,
			const int y,
			const int width,
			const int height,
			const std::uint8_t* src_memory);

		static std::uint16_t interpolate_pixel_linearly_a8r8g8b8_to_v8u8(
			const int x,
			const int y,
			const int width,
			const int height,
			const std::uint8_t* src_memory);
	}; // TextureImpl

	using TextureUPtr = std::unique_ptr<TextureImpl>;
	using TextureUList = std::list<TextureUPtr>;


	using ViewportSize = std::array<int, 2>;
	using Extensions = std::vector<std::string>;

	enum class ShaderType
	{
		none,
		vertex,
		fragment,
	}; // ShaderType


	bool is_initialized_;
	bool is_current_context_;

	HDC ogl_dc_;
	HGLRC ogl_rc_;

	Extensions extensions_;

	bool has_gl_arb_clip_control_;
	bool has_gl_ext_clip_volume_hint_;
	bool has_gl_arb_texture_compression_;
	bool has_gl_ext_texture_compression_s3tc_;

	bool has_gl_ext_texture_filter_anisotropic_;
	float max_anisotropy_;

	int screen_width_;
	int screen_height_;

	bool is_dirty_;

	std::uint8_t clear_color_r_;
	std::uint8_t clear_color_g_;
	std::uint8_t clear_color_b_;
	std::uint8_t clear_color_a_;

	Viewport viewport_;
	ViewportSize max_viewport_size_;

	CullMode cull_mode_;
	FillMode fill_mode_;

	bool is_clipping_;

	bool is_depth_enabled_;
	bool is_depth_writable_;
	CompareFunc depth_func_;

	bool is_blending_enabled_;
	BlendingFactor src_blending_factor_;
	BlendingFactor dst_blending_factor_;


	GLuint vertex_shader_;
	GLuint fragment_shader_;
	GLuint program_;

	bool has_diffuse_;
	GLint u_has_diffuse_;

	bool is_position_transformed_;

	ModelViewMatrixDirtyFlags are_u_model_views_dirty_;
	bool is_u_projection_dirty_;

	WorldMatrices world_matrices_;
	ViewMatrix view_matrix_;
	ProjectionMatrix projection_matrix_;

	UModelViewMatrices u_model_views_;
	GLint u_projection_;

	VertexArrayObjectImpl* current_vao_;

	UiVaos ui_vaos_;
	VertexArrayObjectUList vaos_;

	TextureImpl* current_2d_texture_;
	TextureImpl* current_cube_map_texture_;

	TextureUList textures_;

	SamplerStates sampler_states_;
	float max_texture_lod_bias_;


	static int default_viewport_x;
	static int default_viewport_y;
	static float default_viewport_depth_min_z;
	static float default_viewport_depth_max_z;

	static std::uint8_t default_clear_color_r;
	static std::uint8_t default_clear_color_g;
	static std::uint8_t default_clear_color_b;
	static std::uint8_t default_clear_color_a;

	static const CullMode default_cull_mode;
	static const FillMode default_fill_mode;

	static const bool default_is_clipping;

	static const bool default_is_depth_enabled;
	static const bool default_is_depth_writable;
	static const CompareFunc default_depth_func;

	static const bool default_is_blending_enabled;
	static const BlendingFactor default_src_blending_factor;
	static const BlendingFactor default_dst_blending_factor;

	static const bool default_has_diffuse;

	static const Matrix4F identity_matrix;

	static const std::string vertex_shader_source;
	static const std::string fragment_shader_source;


	// ======================================================================
	// API
	//

	bool do_is_initialized() const override
	{
		return is_initialized_;
	}

	bool do_initialize(
		const int screen_width,
		const int screen_height) override
	{
		if (!initialize_internal(screen_width, screen_height))
		{
			uninitialize_internal();
			return false;
		}

		return true;
	}

	void do_uninitialize() override
	{
		uninitialize_internal();
	}

	bool do_get_is_current_context() const override
	{
		assert(is_initialized_);
		return is_current_context_;
	}

	void do_set_is_current_context(
		const bool is_current) override
	{
		if (!is_initialized_)
		{
			assert(!"Invalid state.");
			return;
		}

		if (is_current == is_current_context_)
		{
			return;
		}

		set_current_context_internal(is_current);
	}

	void do_set_clear_color(
		const std::uint8_t r,
		const std::uint8_t g,
		const std::uint8_t b,
		const std::uint8_t a) override
	{
		if (!is_initialized_ || !is_current_context_)
		{
			assert(!"Invalid state.");
			return;
		}

		if (r == clear_color_r_ &&
			g == clear_color_g_ &&
			b == clear_color_b_ &&
			a == clear_color_a_)
		{
			return;
		}

		clear_color_r_ = r;
		clear_color_g_ = g;
		clear_color_b_ = b;
		clear_color_a_ = a;

		set_clear_color_internal();
	}

	void do_clear(
		const ClearFlags clear_flags) override
	{
		if (!is_initialized_ || !is_current_context_)
		{
			assert(!"Invalid state.");
			return;
		}

		const auto ogl_clear_flags = get_ogl_clear_flags(clear_flags);

		::glClear(ogl_clear_flags);
		assert(ogl_is_succeed());
	}

	const Viewport& do_get_viewport() const override
	{
		assert(is_initialized_);
		return viewport_;
	}

	void do_set_viewport(
		const Viewport& viewport) override
	{
		if (!is_initialized_ || !is_current_context_)
		{
			assert(!"Invalid state.");
			return;
		}

		if (viewport.x_ < 0 ||
			viewport.y_ < 0 ||
			viewport.width_ <= 0 ||
			viewport.height_ <= 0 ||
			viewport.x_ >= max_viewport_size_[0] ||
			viewport.y_ >= max_viewport_size_[1] ||
			viewport.width_ >= max_viewport_size_[0] ||
			viewport.height_ >= max_viewport_size_[1])
		{
			return;
		}

		const auto is_size_changed =
			viewport.x_ != viewport_.x_ ||
			viewport.y_ != viewport_.y_ ||
			viewport.width_ != viewport_.width_ ||
			viewport.height_ != viewport_.height_;

		const auto is_depth_changed =
			viewport.depth_min_z_ != viewport_.depth_min_z_ ||
			viewport.depth_max_z_ != viewport_.depth_max_z_;

		if (!is_size_changed && !is_depth_changed)
		{
			return;
		}

		viewport_ = viewport;

		if (is_size_changed)
		{
			is_dirty_ = true;
			is_u_projection_dirty_ = true;

			set_viewport_size_internal();
		}

		if (is_depth_changed)
		{
			set_viewport_depth_internal();
		}
	}

	CullMode do_get_cull_mode() const override
	{
		assert(is_initialized_);
		return cull_mode_;
	}

	void do_set_cull_mode(
		const CullMode cull_mode) override
	{
		if (!is_initialized_ || !is_current_context_)
		{
			assert(!"Invalid state.");
			return;
		}

		if (cull_mode == cull_mode_)
		{
			return;
		}

		const auto is_old_enabled = (cull_mode_ != CullMode::none);
		const auto is_new_enabled = (cull_mode != CullMode::none);

		cull_mode_ = cull_mode;

		set_cull_mode_internal(is_old_enabled != is_new_enabled);
	}

	FillMode do_get_fill_mode() const override
	{
		assert(is_initialized_);
		return fill_mode_;
	}

	void do_set_fill_mode(
		const FillMode fill_mode) override
	{
		if (!is_initialized_ || !is_current_context_)
		{
			assert(!"Invalid state.");
			return;
		}

		if (fill_mode == fill_mode_)
		{
			return;
		}

		fill_mode_ = fill_mode;

		set_fill_mode_internal();
	}

	bool do_get_is_clipping() const override
	{
		assert(is_initialized_);
		return is_clipping_;
	}

	void do_set_is_clipping(
		const bool is_clipping) override
	{
		if (!is_initialized_ || !is_current_context_)
		{
			assert(!"Invalid state.");
			return;
		}

		if (is_clipping == is_clipping_)
		{
			return;
		}

		is_clipping_ = is_clipping;

		set_is_clipping_internal();
	}

	void set_viewport_size_internal()
	{
		const auto ogl_viewport_y = screen_height_ - (viewport_.y_ + viewport_.height_);

		::glViewport(viewport_.x_, ogl_viewport_y, viewport_.width_, viewport_.height_);
		assert(ogl_is_succeed());
	}

	void set_viewport_depth_internal()
	{
		::glDepthRange(viewport_.depth_min_z_, viewport_.depth_max_z_);
		assert(ogl_is_succeed());
	}

	bool do_is_depth_enabled() const override
	{
		assert(is_initialized_);
		return is_depth_enabled_;
	}

	void do_set_is_depth_enabled(
		const bool is_enabled) override
	{
		if (!is_initialized_ || !is_current_context_)
		{
			assert(!"Invalid state.");
			return;
		}

		if (is_enabled == is_depth_enabled_)
		{
			return;
		}

		is_depth_enabled_ = is_enabled;

		set_is_depth_enabled_internal();

		is_dirty_ = true;
		is_u_projection_dirty_ = true;
	}

	bool do_is_depth_writable() const override
	{
		assert(is_initialized_);
		return is_depth_writable_;
	}

	void do_set_is_depth_writable(
		const bool is_writable) override
	{
		if (!is_initialized_ || !is_current_context_)
		{
			assert(!"Invalid state.");
			return;
		}

		if (is_writable == is_depth_writable_)
		{
			return;
		}

		is_depth_writable_ = is_writable;

		set_is_depth_writable_internal();
	}

	CompareFunc do_get_depth_func() const override
	{
		assert(is_initialized_);
		return depth_func_;
	}

	void do_set_depth_func(
		const CompareFunc depth_func) override
	{
		if (!is_initialized_ || !is_current_context_)
		{
			assert(!"Invalid state.");
			return;
		}

		if (depth_func == depth_func_)
		{
			return;
		}

		depth_func_ = depth_func;

		set_depth_func_internal();
	}

	bool do_get_is_blending_enabled() const override
	{
		assert(is_initialized_);
		return is_blending_enabled_;
	}

	void do_set_is_blending_enabled(
		const bool is_blending_enabled) override
	{
		if (!is_initialized_ || !is_current_context_)
		{
			assert(!"Invalid state.");
			return;
		}

		if (is_blending_enabled_ == is_blending_enabled)
		{
			return;
		}

		set_is_blending_enabled_internal();
	}

	BlendingFactor do_get_src_blending_factor() const override
	{
		assert(is_initialized_);
		return src_blending_factor_;
	}

	BlendingFactor do_get_dst_blending_factor() const override
	{
		assert(is_initialized_);
		return dst_blending_factor_;
	}

	void do_set_blending_factors(
		const BlendingFactor src_factor,
		const BlendingFactor dst_factor) override
	{
		if (!is_initialized_ || !is_current_context_)
		{
			assert(!"Invalid state.");
			return;
		}

		if (src_blending_factor_ == src_factor && dst_blending_factor_ == dst_factor)
		{
			return;
		}

		src_blending_factor_ = src_factor;
		dst_blending_factor_ = dst_factor;

		set_blending_factors_internal();
	}

	const float* do_get_world_matrix(
		const int index) const override
	{
		assert(is_initialized_);

		if (index < 0 || index >= max_world_matrices)
		{
			assert(!"Invalid state.");
			return nullptr;
		}

		return world_matrices_[index].data();
	}

	void do_set_world_matrix(
		const int index,
		const float* const world_matrix_ptr) override
	{
		if (!is_initialized_ || !is_current_context_)
		{
			assert(!"Invalid state.");
			return;
		}

		if (index < 0 ||
			index >= max_world_matrices ||
			!world_matrix_ptr)
		{
			assert(!"Invalid state.");
			return;
		}

		if ((*reinterpret_cast<const Matrix4F*>(world_matrix_ptr)) == world_matrices_[index])
		{
			return;
		}

		world_matrices_[index] = *reinterpret_cast<const Matrix4F*>(world_matrix_ptr);

		is_dirty_ = true;
		are_u_model_views_dirty_.set(index);
	}

	const float* do_get_view_matrix() const override
	{
		assert(is_initialized_);
		return view_matrix_.data();
	}

	void do_set_view_matrix(
		const float* const view_matrix_ptr) override
	{
		if (!is_initialized_ || !is_current_context_)
		{
			assert(!"Invalid state.");
			return;
		}

		if (!view_matrix_ptr)
		{
			assert(!"Invalid state.");
			return;
		}

		if ((*reinterpret_cast<const ViewMatrix*>(view_matrix_ptr)) == view_matrix_)
		{
			return;
		}

		view_matrix_ = *reinterpret_cast<const ViewMatrix*>(view_matrix_ptr);

		is_dirty_ = true;
		are_u_model_views_dirty_.set();
	}

	const float* do_get_projection_matrix() const override
	{
		assert(is_initialized_);
		return projection_matrix_.data();
	}

	void do_set_projection_matrix(
		const float* const projection_matrix_ptr) override
	{
		if (!is_initialized_ || !is_current_context_)
		{
			assert(!"Invalid state.");
			return;
		}

		if (!projection_matrix_ptr)
		{
			assert(!"Invalid state.");
			return;
		}

		if ((*reinterpret_cast<const ProjectionMatrix*>(projection_matrix_ptr)) == projection_matrix_)
		{
			return;
		}

		projection_matrix_ = *reinterpret_cast<const ProjectionMatrix*>(projection_matrix_ptr);

		is_dirty_ = true;
		is_u_projection_dirty_ = true;
	}

	SamplerStatePtr do_get_sampler_state(
		const int index) override
	{
		return &sampler_states_[index];
	}

	VertexArrayObjectPtr do_add_vertex_array_object() override
	{
		vaos_.emplace_back(std::make_unique<VertexArrayObjectImpl>(*this));
		return vaos_.back().get();
	}

	void do_remove_vertex_array_object(
		VertexArrayObjectPtr vertex_array_object) override
	{
		if (vaos_.empty())
		{
			assert(!"Empty list.");
			return;
		}

		const auto size_before = vaos_.size();

		vaos_.remove_if(
			[&](const auto& item_uptr)
			{
				return item_uptr.get() == vertex_array_object;
			}
		);

		const auto size_after = vaos_.size();

		assert(size_before > size_after);
	}

	TexturePtr do_add_texture() override
	{
		textures_.emplace_back(std::make_unique<TextureImpl>(*this));
		return textures_.back().get();
	}

	void do_remove_texture(
		TexturePtr texture) override
	{
		if (textures_.empty())
		{
			assert(!"Empty list.");
			return;
		}

		const auto size_before = textures_.size();

		auto old_is_current_context = false;

		auto guard_context = ul::ScopeGuard{
			[&]()
			{
				old_is_current_context = set_post_is_current_context(true);
			},

			[&]()
			{
				set_is_current_context(old_is_current_context);
			},
		};

		textures_.remove_if(
			[&](const auto& item_uptr)
			{
				return item_uptr.get() == texture;
			}
		);

		const auto size_after = textures_.size();

		assert(size_before > size_after);
	}

	void do_draw(
		const PrimitiveType primitive_type,
		const std::uint32_t d3d_fvf,
		const int primitive_count,
		const void* const raw_data) override
	{
		if (!is_initialized_ || !is_current_context_ || !raw_data)
		{
			assert(!"Invalid state.");
			return;
		}


		auto ui_vao_end_it = ui_vaos_.end();

		auto ui_vao_it = std::find_if(
			ui_vaos_.begin(),
			ui_vao_end_it,
			[&](const auto& item)
			{
				return item.d3d_fvf_ == d3d_fvf;
			}
		);

		if (ui_vao_it == ui_vao_end_it)
		{
			return;
		}

		auto& ui_vao = *ui_vao_it;

		auto vao = ui_vao.vao_;

		if (!vao)
		{
			return;
		}

		const auto vertex_count = calculate_vertex_count(primitive_type, primitive_count);

		if (vertex_count <= 0)
		{
			return;
		}

		const auto end_vertex_offset = ui_vao.vertex_offset_ + vertex_count;

		if (end_vertex_offset >= max_ui_vao_vertices)
		{
			ui_vao.vertex_offset_ = 0;
		}

		vao->set_vertex_data(ui_vao.vertex_offset_, vertex_count, raw_data);
		vao->draw(primitive_type, ui_vao.vertex_offset_, primitive_count);

		ui_vao.vertex_offset_ += vertex_count;
	}

	//
	// API
	// ======================================================================


	bool initialize_internal(
		const int screen_width,
		const int screen_height)
	{
		ogl_clear_error();

		if (screen_width <= 0 || screen_height <= 0)
		{
			return false;
		}

		screen_width_ = screen_width;
		screen_height_ = screen_height;

		// Get current context (must exist).
		//
		ogl_dc_ = ::wglGetCurrentDC();
		ogl_rc_ = ::wglGetCurrentContext();

		if (!ogl_dc_ || !ogl_rc_)
		{
			return false;
		}

		detect_extensions();
		get_impl_defined_values();

		if (!load_program())
		{
			return false;
		}

		if (!enable_program(true))
		{
			return false;
		}

		if (!locate_uniforms())
		{
			return false;
		}

		if (!initialize_samplers())
		{
			return false;
		}

		// Set defaults.
		//
		if (has_gl_arb_clip_control_)
		{
			::glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
			assert(ogl_is_succeed());
		}

		set_default_clear_color();
		set_default_viewport();
		set_default_cull_mode();
		set_default_fill_mode();
		set_default_is_clipping();
		set_default_is_depth_enabled();
		set_default_is_depth_writable();
		set_default_depth_func();
		set_default_blending();
		set_default_world_matrices();
		set_default_view_matrix();
		set_default_projection_matrix();
		set_uniform_defaults();

		if (!ogl_is_succeed())
		{
			assert(!"OpenGL error.");
			return false;
		}

		is_initialized_ = true;
		is_current_context_ = true;

		add_ui_vaos();
	
		return true;
	}

	void uninitialize_internal()
	{
		is_initialized_ = false;
		is_current_context_ = false;

		ogl_dc_ = nullptr;
		ogl_rc_ = nullptr;

		extensions_.clear();

		has_gl_arb_clip_control_ = false;
		has_gl_ext_clip_volume_hint_ = false;
		has_gl_arb_texture_compression_ = false;
		has_gl_ext_texture_compression_s3tc_ = false;

		has_gl_ext_texture_filter_anisotropic_ = false;
		max_anisotropy_ = 0.0F;

		screen_width_ = 0;
		screen_height_ = 0;

		is_dirty_ = false;

		clear_color_r_ = 0;
		clear_color_g_ = 0;
		clear_color_b_ = 0;
		clear_color_a_ = 0;

		viewport_ = {};
		max_viewport_size_ = {};

		cull_mode_ = CullMode::none;
		fill_mode_ = FillMode::none;

		is_clipping_ = false;

		is_depth_enabled_ = false;
		is_depth_writable_ = false;
		depth_func_ = CompareFunc::none;

		is_blending_enabled_ = false;
		src_blending_factor_ = BlendingFactor::none;
		dst_blending_factor_ = BlendingFactor::none;

		if (program_)
		{
			::glUseProgram(0);
			assert(ogl_is_succeed());
			::glDetachShader(program_, vertex_shader_);
			assert(ogl_is_succeed());
			::glDetachShader(program_, fragment_shader_);
			assert(ogl_is_succeed());
			::glDeleteProgram(program_);
			assert(ogl_is_succeed());

			program_ = 0;
		}

		has_diffuse_ = false;
		u_has_diffuse_ = -1;

		is_position_transformed_ = false;

		are_u_model_views_dirty_ = {};
		is_u_projection_dirty_ = false;

		world_matrices_ = {};
		view_matrix_ = {};
		projection_matrix_ = {};

		u_model_views_.fill(-1);
		u_projection_ = -1;

		if (vertex_shader_)
		{
			::glDeleteShader(vertex_shader_);
			assert(ogl_is_succeed());

			vertex_shader_ = 0;
		}

		if (fragment_shader_)
		{
			::glDeleteShader(fragment_shader_);
			assert(ogl_is_succeed());

			fragment_shader_ = 0;
		}

		current_vao_ = nullptr;

		ui_vaos_.fill({});
		vaos_.clear();

		current_2d_texture_ = nullptr;
		current_cube_map_texture_ = nullptr;

		textures_.clear();

		uninitialize_samplers();
		max_texture_lod_bias_ = 0.0F;
	}

	void set_current_context_internal(
		const bool is_current)
	{
		const auto dc = (is_current ? ogl_dc_ : nullptr);
		const auto rc = (is_current ? ogl_rc_ : nullptr);

		const auto make_result = ::wglMakeCurrent(dc, rc);
		assert(make_result);

		is_current_context_ = is_current;
	}

	void set_default_cull_mode()
	{
		::glFrontFace(GL_CW);
		assert(ogl_is_succeed());

		cull_mode_ = default_cull_mode;
		set_cull_mode_internal(true);
	}

	void set_cull_mode_internal(
		const bool enforce_cull_face)
	{
		const auto is_cull_face_enabled = (cull_mode_ != CullMode::none);

		switch (cull_mode_)
		{
		case CullMode::none:
			break;

		case CullMode::cw:
			::glCullFace(GL_FRONT);
			assert(ogl_is_succeed());
			break;

		case CullMode::ccw:
			::glCullFace(GL_BACK);
			assert(ogl_is_succeed());
			break;

		default:
			assert(!"Unsupported culling mode.");
			return;
		}

		if (enforce_cull_face)
		{
			if (is_cull_face_enabled)
			{
				::glEnable(GL_CULL_FACE);
				assert(ogl_is_succeed());
			}
			else
			{
				::glDisable(GL_CULL_FACE);
				assert(ogl_is_succeed());
			}
		}
	}

	void set_default_fill_mode()
	{
		fill_mode_ = default_fill_mode;
		set_fill_mode_internal();
	}

	void set_fill_mode_internal()
	{
		const auto ogl_fill_mode = get_ogl_fill_mode(fill_mode_);

		::glPolygonMode(GL_FRONT_AND_BACK, ogl_fill_mode);
		assert(ogl_is_succeed());
	}

	void set_default_is_clipping()
	{
		is_clipping_ = default_is_clipping;
		set_is_clipping_internal();
	}

	void set_is_clipping_internal()
	{
		if (!has_gl_ext_clip_volume_hint_)
		{
			return;
		}

		const auto ogl_hint_value = (is_clipping_ ? GL_DONT_CARE : GL_FASTEST);

		::glHint(GL_CLIP_VOLUME_CLIPPING_HINT_EXT, ogl_hint_value);
		assert(ogl_is_succeed());
	}

	void set_default_clear_color()
	{
		clear_color_r_ = default_clear_color_r;
		clear_color_g_ = default_clear_color_g;
		clear_color_b_ = default_clear_color_b;
		clear_color_a_ = default_clear_color_a;

		set_clear_color_internal();
	}

	void set_clear_color_internal()
	{
		::glClearColor(
			clear_color_r_ / 255.0F,
			clear_color_g_ / 255.0F,
			clear_color_b_ / 255.0F,
			clear_color_a_ / 255.0F);

		assert(ogl_is_succeed());
	}

	Viewport get_default_viewport()
	{
		auto viewport = Viewport{};
		viewport.x_ = default_viewport_x;
		viewport.y_ = default_viewport_y;
		viewport.width_ = screen_width_;
		viewport.height_ = screen_height_;
		viewport.depth_min_z_ = default_viewport_depth_min_z;
		viewport.depth_max_z_ = default_viewport_depth_max_z;

		return viewport;
	}

	void set_default_viewport()
	{
		viewport_ = get_default_viewport();

		set_viewport_size_internal();
		set_viewport_depth_internal();
	}

	void set_default_is_depth_enabled()
	{
		is_depth_enabled_ = default_is_depth_enabled;
		set_is_depth_enabled_internal();
	}

	void set_is_depth_enabled_internal()
	{
		if (is_depth_enabled_)
		{
			::glEnable(GL_DEPTH_TEST);
			assert(ogl_is_succeed());
		}
		else
		{
			::glDisable(GL_DEPTH_TEST);
			assert(ogl_is_succeed());
		}
	}

	void set_default_is_depth_writable()
	{
		is_depth_writable_ = default_is_depth_writable;
		set_is_depth_writable_internal();
	}

	void set_is_depth_writable_internal()
	{
		::glDepthMask(is_depth_writable_);
		assert(ogl_is_succeed());
	}

	void set_default_depth_func()
	{
		depth_func_ = default_depth_func;
		set_depth_func_internal();
	}

	void set_depth_func_internal()
	{
		const auto ogl_depth_func = get_ogl_compare_func(depth_func_);

		::glDepthFunc(ogl_depth_func);
		assert(ogl_is_succeed());
	}

	void set_is_blending_enabled_internal()
	{
		if (is_blending_enabled_)
		{
			::glEnable(GL_BLEND);
			assert(ogl_is_succeed());
		}
		else
		{
			::glDisable(GL_BLEND);
			assert(ogl_is_succeed());
		}
	}

	void set_blending_factors_internal()
	{
		const auto ogl_src_factor = get_ogl_blending_factor(src_blending_factor_);
		const auto ogl_dst_factor = get_ogl_blending_factor(dst_blending_factor_);

		::glBlendFunc(ogl_src_factor, ogl_dst_factor);
		assert(ogl_is_succeed());
	}

	void set_default_blending()
	{
		is_blending_enabled_ = default_is_blending_enabled;
		set_is_blending_enabled_internal();

		src_blending_factor_ = default_src_blending_factor;
		dst_blending_factor_ = default_dst_blending_factor;
		set_blending_factors_internal();
	}

	void set_uniform_defaults()
	{
		has_diffuse_ = default_has_diffuse;
		set_has_diffuse_internal();
	}

	void add_ui_vaos()
	{
		auto param = VertexArrayObject::InitializeParam{};
		param.vertex_count_ = max_ui_vao_vertices;
		param.vertex_usage_flags_ = VertexArrayObject::UsageFlags::is_dynamic;

		for (auto i = 0; i < max_ui_vaos; ++i)
		{
			auto vao = add_vertex_array_object();

			if (vao)
			{
				ui_vaos_[i].vao_ = vao;

				param.vertex_format_ = ui_vao_d3d_fvfs[i];

				if (vao->initialize(param))
				{
					ui_vaos_[i].d3d_fvf_ = ui_vao_d3d_fvfs[i];
				}
				else
				{
					remove_vertex_array_object(vao);
					ui_vaos_[i].vao_ = nullptr;
				}
			}
		}
	}

	void get_extensions()
	{
		auto extension_count = GLint{};

		::glGetIntegerv(GL_NUM_EXTENSIONS, &extension_count);

		extensions_.reserve(extension_count);

		for (auto i = 0; i < extension_count; ++i)
		{
			const auto extension_name = reinterpret_cast<const char*>(::glGetStringi(GL_EXTENSIONS, i));

			assert(extension_name);

			if (extension_name)
			{
				extensions_.emplace_back(extension_name);
			}
		}
	}

	bool has_extension(
		const std::string& extension_name)
	{
		if (extensions_.empty() || extension_name.empty())
		{
			return false;
		}

		const auto extension_end_it = extensions_.cend();

		const auto extension_it = std::find_if(
			extensions_.cbegin(),
			extension_end_it,
			[&](const auto& item)
			{
				return item == extension_name;
			}
		);

		return extension_it != extension_end_it;
	}

	void detect_gl_arb_clip_control_extension()
	{
		has_gl_arb_clip_control_ = has_extension("GL_ARB_clip_control");
		::glClipControl = ogl_resolve_symbol<PFNGLCLIPCONTROLPROC>("glClipControl");

		if (!has_gl_arb_clip_control_ || !::glClipControl)
		{
			has_gl_arb_clip_control_ = false;
			::glClipControl = nullptr;
		}
	}

	void detect_gl_ext_clip_volume_hint_extension()
	{
		has_gl_ext_clip_volume_hint_ = has_extension("GL_EXT_clip_volume_hint");
	}

	void detect_gl_arb_texture_compression_extension()
	{
		has_gl_arb_texture_compression_ = has_extension("GL_ARB_texture_compression");
	}

	void detect_gl_ext_texture_compression_s3tc_extension()
	{
		has_gl_ext_texture_compression_s3tc_ = has_extension("GL_EXT_texture_compression_s3tc");
	}

	void detect_gl_ext_texture_filter_anisotropic_extension()
	{
		auto is_succeed = false;

		has_gl_ext_texture_filter_anisotropic_ = has_extension("GL_EXT_texture_filter_anisotropic");

		if (has_gl_ext_texture_filter_anisotropic_)
		{
			::glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_anisotropy_);

			if (ogl_is_succeed())
			{
				if (max_anisotropy_ > min_anisotropy)
				{
					is_succeed = true;
				}
			}
		}

		if (!is_succeed)
		{
			has_gl_ext_texture_filter_anisotropic_ = false;
			max_anisotropy_ = 0.0F;
		}
	}


	void detect_extensions()
	{
		get_extensions();

		detect_gl_arb_clip_control_extension();
		detect_gl_ext_clip_volume_hint_extension();
		detect_gl_arb_texture_compression_extension();
		detect_gl_ext_texture_compression_s3tc_extension();
		detect_gl_ext_texture_filter_anisotropic_extension();
	}

	bool supports_dxtc() const
	{
		return has_gl_arb_texture_compression_ && has_gl_ext_texture_compression_s3tc_;
	}

	bool get_impl_defined_viewport_values()
	{
		::glGetIntegerv(GL_MAX_VIEWPORT_DIMS, max_viewport_size_.data());

		if (!ogl_is_succeed())
		{
			assert(!"Failed to get max viewport dimensions.");
			return false;
		}

		if (max_viewport_size_[0] <= 0 ||
			max_viewport_size_[1] <= 0 ||
			screen_width_ > max_viewport_size_[0] ||
			screen_height_ > max_viewport_size_[1])
		{
			assert(!"Viewport size out of range.");
			return false;
		}

		return true;
	}

	bool get_impl_defined_texture_lod_bias_values()
	{
		::glGetFloatv(GL_MAX_TEXTURE_LOD_BIAS, &max_texture_lod_bias_);

		if (!ogl_is_succeed())
		{
			assert(!"Failed to get max texture LOD bias.");
			return false;
		}

		return true;
	}

	bool get_impl_defined_values()
	{
		if (!get_impl_defined_viewport_values())
		{
			return false;
		}

		if (!get_impl_defined_texture_lod_bias_values())
		{
			return false;
		}

		return true;
	}

	void get_shader_build_status(
		const GLenum object_status,
		void (APIENTRYP ogl_get_parameter)(GLuint object, GLenum pname, GLint* params),
		void (APIENTRYP ogl_get_log)(GLuint object, GLsizei bufSize, GLsizei* length, GLchar* infoLog),
		const GLuint object,
		bool& is_built,
		std::string& log)
	{
		is_built = false;
		log.clear();

		auto ogl_compile_status = GLint{};
		auto ogl_log_length = GLint{};

		ogl_get_parameter(object, object_status, &ogl_compile_status);
		ogl_get_parameter(object, GL_INFO_LOG_LENGTH, &ogl_log_length);

		is_built = (ogl_compile_status == GL_TRUE);

		if (ogl_log_length > 0)
		{
			log.resize(ogl_log_length);
			ogl_get_log(object, ogl_log_length, &ogl_log_length, &log[0]);
		}
	}

	void get_shader_compile_status(
		const GLuint shader,
		bool& is_compiled,
		std::string& log)
	{
		get_shader_build_status(GL_COMPILE_STATUS, ::glGetShaderiv, ::glGetShaderInfoLog, shader, is_compiled, log);
	}

	void get_program_link_status(
		const GLuint program,
		bool& is_linked,
		std::string& log)
	{
		get_shader_build_status(GL_LINK_STATUS, ::glGetProgramiv, ::glGetProgramInfoLog, program, is_linked, log);
	}

	bool load_shader(
		const ShaderType shader_type,
		GLuint& shader)
	{
		shader = GL_NONE;

		const std::string* shader_source;
		GLenum ogl_shader_type;

		switch (shader_type)
		{
		case ShaderType::vertex:
			ogl_shader_type = GL_VERTEX_SHADER;
			shader_source = &vertex_shader_source;
			break;

		case ShaderType::fragment:
			ogl_shader_type = GL_FRAGMENT_SHADER;
			shader_source = &fragment_shader_source;
			break;

		default:
			assert(!"Unsupported shader type.");
			return false;
		}

		shader = ::glCreateShader(ogl_shader_type);
		assert(ogl_is_succeed());

		if (shader == 0)
		{
			return false;
		}

		const char* const source_lines[1] =
		{
			shader_source->c_str(),
		}; // source_lines

		const GLint source_lines_lengths[1] =
		{
			static_cast<GLint>(shader_source->length()),
		}; // source_lines

		::glShaderSource(shader, 1, source_lines, source_lines_lengths);
		assert(ogl_is_succeed());
		::glCompileShader(shader);
		assert(ogl_is_succeed());

		bool is_compiled;
		std::string log;

		get_shader_compile_status(shader, is_compiled, log);

		return is_compiled;
	}

	bool load_program()
	{
		if (!load_shader(ShaderType::vertex, vertex_shader_))
		{
			return false;
		}

		if (!load_shader(ShaderType::fragment, fragment_shader_))
		{
			return false;
		}

		program_ = ::glCreateProgram();

		if (!program_)
		{
			return false;
		}

		::glAttachShader(program_, vertex_shader_);
		assert(ogl_is_succeed());
		::glAttachShader(program_, fragment_shader_);
		assert(ogl_is_succeed());
		::glLinkProgram(program_);
		assert(ogl_is_succeed());

		bool is_linked;
		std::string log;

		get_program_link_status(program_, is_linked, log);

		return is_linked;
	}

	bool enable_program(
		const bool is_enable)
	{
		::glUseProgram(is_enable ? program_ : 0);

		return ogl_is_succeed();
	}

	bool locate_uniforms()
	{
		u_has_diffuse_ = ::glGetUniformLocation(program_, "u_has_diffuse");
		assert(ogl_is_succeed());
		assert(u_has_diffuse_ >= 0);

		for (auto i = 0; i < max_world_matrices; ++i)
		{
			const auto uniform_name = "u_model_views[" + std::to_string(i) + "]";
			u_model_views_[i] = ::glGetUniformLocation(program_, uniform_name.c_str());
			assert(ogl_is_succeed());
			assert(u_model_views_[i] >= 0);
		}

		u_projection_ = ::glGetUniformLocation(program_, "u_projection");
		assert(ogl_is_succeed());
		assert(u_projection_ >= 0);

		return ogl_is_succeed();
	}

	void set_has_diffuse(
		const bool has_diffuse)
	{
		if (!is_initialized_)
		{
			return;
		}

		if (has_diffuse == has_diffuse_)
		{
			return;
		}

		has_diffuse_ = has_diffuse;

		set_has_diffuse_internal();
	}

	void set_has_diffuse_internal()
	{
		::glUniform1i(u_has_diffuse_, has_diffuse_);
		assert(ogl_is_succeed());
	}

	void set_is_position_transformed(
		const bool is_position_transformed)
	{
		if (!is_initialized_)
		{
			return;
		}

		if (is_position_transformed == is_position_transformed_)
		{
			return;
		}

		is_position_transformed_ = is_position_transformed;

		is_dirty_ = true;
		are_u_model_views_dirty_.set();
		is_u_projection_dirty_ = true;
	}

	void set_u_model_view(
		const int index)
	{
		ModelViewMatrix matrix;

		if (is_position_transformed_)
		{
			matrix = identity_matrix;
		}
		else
		{
			matrix = multiply_matrices(world_matrices_[index], view_matrix_);
		}

		::glUniformMatrix4fv(u_model_views_[index], 1, GL_FALSE, matrix.data());
		assert(ogl_is_succeed());
	}

	void set_u_projection()
	{
		ProjectionMatrix matrix;

		if (is_position_transformed_)
		{
			const auto m11 = 2.0F / viewport_.width_;
			const auto m41 = -1.0F;

			const auto m22 = -2.0F / viewport_.height_;
			const auto m42 = 1.0F;

			const auto m33 = 0.0F;
			const auto m43 = 0.0F;

			matrix =
			{
				m11, 0.0F, 0.0F, 0.0F,
				0.0F, m22, 0.0F, 0.0F,
				0.0F, 0.0F, m33, 0.0F,
				m41, m42, m43, 1.0F,
			};
		}
		else
		{
			matrix = projection_matrix_;
		}

		::glUniformMatrix4fv(u_projection_, 1, GL_FALSE, matrix.data());
		assert(ogl_is_succeed());
	}

	void set_default_world_matrices()
	{
		world_matrices_.fill(identity_matrix);

		is_dirty_ = true;
		are_u_model_views_dirty_.set();
	}

	void set_default_view_matrix()
	{
		view_matrix_ = identity_matrix;

		is_dirty_ = true;
		are_u_model_views_dirty_.set();
	}

	void set_default_projection_matrix()
	{
		projection_matrix_ = identity_matrix;

		is_dirty_ = true;
		is_u_projection_dirty_ = true;
	}

	void apply_u_view_model_changes(
		const int index)
	{
		if (!are_u_model_views_dirty_.test(index))
		{
			return;
		}

		are_u_model_views_dirty_.reset(index);

		set_u_model_view(index);
	}

	void apply_u_view_models_changes()
	{
		for (auto i = 0; i < max_world_matrices; ++i)
		{
			apply_u_view_model_changes(i);
		}
	}

	void apply_u_projection_changes()
	{
		if (!is_u_projection_dirty_)
		{
			return;
		}

		is_u_projection_dirty_ = false;

		set_u_projection();
	}

	void apply_changes()
	{
		if (!is_dirty_)
		{
			return;
		}

		is_dirty_ = false;

		apply_u_view_models_changes();
		apply_u_projection_changes();
	}

	static Matrix4F multiply_matrices(
		const Matrix4F& lhs,
		const Matrix4F& rhs)
	{
		return {
			(rhs[0] * lhs[0]) + (rhs[4] * lhs[1]) + (rhs[8] * lhs[2]) + (rhs[12] * lhs[3]),
			(rhs[1] * lhs[0]) + (rhs[5] * lhs[1]) + (rhs[9] * lhs[2]) + (rhs[13] * lhs[3]),
			(rhs[2] * lhs[0]) + (rhs[6] * lhs[1]) + (rhs[10] * lhs[2]) + (rhs[14] * lhs[3]),
			(rhs[3] * lhs[0]) + (rhs[7] * lhs[1]) + (rhs[11] * lhs[2]) + (rhs[15] * lhs[3]),
			(rhs[0] * lhs[4]) + (rhs[4] * lhs[5]) + (rhs[8] * lhs[6]) + (rhs[12] * lhs[7]),
			(rhs[1] * lhs[4]) + (rhs[5] * lhs[5]) + (rhs[9] * lhs[6]) + (rhs[13] * lhs[7]),
			(rhs[2] * lhs[4]) + (rhs[6] * lhs[5]) + (rhs[10] * lhs[6]) + (rhs[14] * lhs[7]),
			(rhs[3] * lhs[4]) + (rhs[7] * lhs[5]) + (rhs[11] * lhs[6]) + (rhs[15] * lhs[7]),
			(rhs[0] * lhs[8]) + (rhs[4] * lhs[9]) + (rhs[8] * lhs[10]) + (rhs[12] * lhs[11]),
			(rhs[1] * lhs[8]) + (rhs[5] * lhs[9]) + (rhs[9] * lhs[10]) + (rhs[13] * lhs[11]),
			(rhs[2] * lhs[8]) + (rhs[6] * lhs[9]) + (rhs[10] * lhs[10]) + (rhs[14] * lhs[11]),
			(rhs[3] * lhs[8]) + (rhs[7] * lhs[9]) + (rhs[11] * lhs[10]) + (rhs[15] * lhs[11]),
			(rhs[0] * lhs[12]) + (rhs[4] * lhs[13]) + (rhs[8] * lhs[14]) + (rhs[12] * lhs[15]),
			(rhs[1] * lhs[12]) + (rhs[5] * lhs[13]) + (rhs[9] * lhs[14]) + (rhs[13] * lhs[15]),
			(rhs[2] * lhs[12]) + (rhs[6] * lhs[13]) + (rhs[10] * lhs[14]) + (rhs[14] * lhs[15]),
			(rhs[3] * lhs[12]) + (rhs[7] * lhs[13]) + (rhs[11] * lhs[14]) + (rhs[15] * lhs[15]),
		};
	}

	bool initialize_samplers()
	{
		auto param = SamplerStateImpl::InitializeParam{};
		param.has_anisotropy_ = has_gl_ext_texture_filter_anisotropic_;
		param.max_anisotropy_ = max_anisotropy_;
		param.max_lod_bias_ = max_texture_lod_bias_;

		for (auto i = 0; i < max_samplers; ++i)
		{
			param.unit_index_ = i;

			if (!sampler_states_[i].initialize(param))
			{
				return false;
			}
		}

		return true;
	}

	void uninitialize_samplers()
	{
		for (auto& sampler_state : sampler_states_)
		{
			sampler_state.uninitialize();
		}
	}

	static GLbitfield get_ogl_clear_flags(
		const ClearFlags d3d_clear_flags)
	{
		auto ogl_clear_flags = GLbitfield{};

		if ((d3d_clear_flags & ClearFlags::target) != 0)
		{
			ogl_clear_flags |= GL_COLOR_BUFFER_BIT;
		}

		if ((d3d_clear_flags & ClearFlags::zbuffer) != 0)
		{
			ogl_clear_flags |= GL_DEPTH_BUFFER_BIT;
		}

		if ((d3d_clear_flags & ClearFlags::stencil) != 0)
		{
			ogl_clear_flags |= GL_STENCIL_BUFFER_BIT;
		}

		return ogl_clear_flags;
	}

	static GLenum get_ogl_fill_mode(
		const FillMode d3d_fill_mode)
	{
		switch (d3d_fill_mode)
		{
		case FillMode::wireframe:
			return GL_LINE;

		case FillMode::solid:
			return GL_FILL;

		default:
			assert(!"Unsupported fill mode.");
			return invalid_ogl_enum;
		}
	}

	static GLenum get_ogl_compare_func(
		const CompareFunc d3d_depth_func)
	{
		switch (d3d_depth_func)
		{
		case CompareFunc::always:
			return GL_ALWAYS;

		case CompareFunc::equal:
			return GL_EQUAL;

		case CompareFunc::greater:
			return GL_GREATER;

		case CompareFunc::greater_or_equal:
			return GL_GEQUAL;

		case CompareFunc::less:
			return GL_LESS;

		case CompareFunc::less_or_equal:
			return GL_LEQUAL;

		case CompareFunc::not_equal:
			return GL_NOTEQUAL;

		default:
			assert(!"Invalid compare function.");
			return invalid_ogl_enum;
		}
	}

	static GLenum get_ogl_blending_factor(
		const BlendingFactor blending_factor)
	{
		switch (blending_factor)
		{
			case OglRenderer::BlendingFactor::zero:
				return GL_ZERO;

			case OglRenderer::BlendingFactor::one:
				return GL_ONE;

			case OglRenderer::BlendingFactor::src_alpha:
				return GL_SRC_ALPHA;

			case OglRenderer::BlendingFactor::src_color:
				return GL_SRC_COLOR;

			case OglRenderer::BlendingFactor::inv_src_alpha:
				return GL_ONE_MINUS_SRC_ALPHA;

			case OglRenderer::BlendingFactor::inv_src_color:
				return GL_ONE_MINUS_SRC_COLOR;

			case OglRenderer::BlendingFactor::dst_color:
				return GL_DST_COLOR;

			case OglRenderer::BlendingFactor::inv_dst_color:
				return GL_ONE_MINUS_DST_COLOR;

		default:
			assert(!"Unsupported blending factor.");
			return invalid_ogl_enum;
		}
	}

	static int calculate_vertex_count(
		const PrimitiveType primitive_type,
		const int primitive_count)
	{
		if (primitive_count <= 0)
		{
			assert(!"Invalid state.");
			return 0;
		}

		if (primitive_count == 1)
		{
			return 3;
		}

		switch (primitive_type)
		{
		case PrimitiveType::triangle_fan:
		case PrimitiveType::triangle_strip:
			return 3 + (primitive_count - 1);

		case PrimitiveType::triangle_list:
			return 3 * primitive_count;

		default:
			assert(!"Unsupported primitive type.");
			return 0;
		}
	}

	static GLenum get_ogl_primitive_type(
		const PrimitiveType primitive_type)
	{
		switch (primitive_type)
		{
		case PrimitiveType::triangle_fan:
			return GL_TRIANGLE_FAN;

		case PrimitiveType::triangle_strip:
			return GL_TRIANGLE_STRIP;

		case PrimitiveType::triangle_list:
			return GL_TRIANGLES;

		default:
			assert(!"Unsupported primitive type.");
			return invalid_ogl_enum;
		}
	}
}; // OglRendererImpl


int OglRendererImpl::default_viewport_x = 0;
int OglRendererImpl::default_viewport_y = 0;
float OglRendererImpl::default_viewport_depth_min_z = 0.0F;
float OglRendererImpl::default_viewport_depth_max_z = 1.0F;

std::uint8_t OglRendererImpl::default_clear_color_r = 0;
std::uint8_t OglRendererImpl::default_clear_color_g = 0;
std::uint8_t OglRendererImpl::default_clear_color_b = 0;
std::uint8_t OglRendererImpl::default_clear_color_a = 0;

const OglRenderer::CullMode OglRendererImpl::default_cull_mode = OglRenderer::CullMode::ccw;
const OglRenderer::FillMode OglRendererImpl::default_fill_mode = OglRenderer::FillMode::solid;

const bool OglRendererImpl::default_is_clipping = true;

const bool OglRendererImpl::default_is_depth_enabled = true;
const bool OglRendererImpl::default_is_depth_writable = true;
const OglRendererImpl::CompareFunc OglRendererImpl::default_depth_func = OglRenderer::CompareFunc::less_or_equal;

const bool OglRendererImpl::default_is_blending_enabled = false;
const OglRenderer::BlendingFactor OglRendererImpl::default_src_blending_factor = OglRenderer::BlendingFactor::one;
const OglRenderer::BlendingFactor OglRendererImpl::default_dst_blending_factor = OglRenderer::BlendingFactor::zero;

const bool OglRendererImpl::default_has_diffuse = false;

const OglRendererImpl::Matrix4F OglRendererImpl::identity_matrix =
{
	1.0F, 0.0F, 0.0F, 0.0F,
	0.0F, 1.0F, 0.0F, 0.0F,
	0.0F, 0.0F, 1.0F, 0.0F,
	0.0F, 0.0F, 0.0F, 1.0F,
};

const std::string OglRendererImpl::vertex_shader_source = std::string{
R"LTJS_VERTEX(

#version 330 core

// Maximum model-view matrix count.
const int max_model_view_count = 4;

// Maximum texture coordinate sets.
const int max_tcs = 4;

// Default diffuse color.
const vec4 default_diffuse = vec4(1, 1, 1, 1);


layout(location = 0) in vec4 a_position;
layout(location = 1) in vec4 a_bweights;
layout(location = 2) in vec4 a_normal;
layout(location = 3) in vec4 a_diffuse;
layout(location = 4) in vec4 a_tcs[max_tcs];


out vec4 v_diffuse;


// Has diffuse attribute?
uniform bool u_has_diffuse = false;


// Model-view matrices.
uniform mat4 u_model_views[max_model_view_count];

// Projection matrix.
uniform mat4 u_projection;


void main()
{
	if (u_has_diffuse)
	{
		v_diffuse = a_diffuse;
	}
	else
	{
		v_diffuse = default_diffuse;
	}

	gl_Position = u_projection * u_model_views[0] * a_position;
}

)LTJS_VERTEX"
}; // vertex_shader_source

const std::string OglRendererImpl::fragment_shader_source = std::string{
R"LTJS_FRAGMENT(

#version 330 core

in vec4 v_diffuse;

out vec4 o_fragment;

void main()
{
	o_fragment = v_diffuse;
}

)LTJS_FRAGMENT"
}; // fragment_shader_source

//
// OglRendererImpl
// ==========================================================================


// ==========================================================================
// OglRendererImpl::VertexArrayObjectImpl
//

OglRendererImpl::VertexArrayObjectImpl::VertexArrayObjectImpl(
	OglRendererImpl& impl)
	:
	ogl_renderer_{impl},
	is_initialized_{},
	vertex_format_{},
	vertex_usage_flags_{},
	index_usage_flags_{},
	vertex_count_{},
	index_count_{},
	ogl_vao_{},
	ogl_vertex_buffer_{},
	ogl_index_buffer_{}
{
}

OglRendererImpl::VertexArrayObjectImpl::~VertexArrayObjectImpl()
{
	uninitialize_internal();
}

bool OglRendererImpl::VertexArrayObjectImpl::do_initialize(
	const InitializeParam& param)
{
	uninitialize_internal();

	if (!param.is_valid())
	{
		return false;
	}

	if (!initialize_internal(param))
	{
		uninitialize_internal();
		return false;
	}

	return true;
}

bool OglRendererImpl::VertexArrayObjectImpl::initialize_internal(
	const InitializeParam& param)
{
	::glGenVertexArrays(1, &ogl_vao_);
	assert(ogl_is_succeed());
	::glGenBuffers(1, &ogl_vertex_buffer_);
	assert(ogl_is_succeed());

	if (param.has_index_)
	{
		::glGenBuffers(1, &ogl_index_buffer_);
		assert(ogl_is_succeed());
	}


	// Vertex buffer.
	//
	const auto ogl_vertex_usage = usage_flags_to_ogl_usage(param.vertex_usage_flags_);
	const auto vertex_size = param.vertex_format_.vertex_size_;
	const auto vertex_buffer_size = vertex_size * param.vertex_count_;

	::glBindBuffer(GL_ARRAY_BUFFER, ogl_vertex_buffer_);
	assert(ogl_is_succeed());
	::glBufferData(GL_ARRAY_BUFFER, vertex_buffer_size, param.raw_vertex_data_, ogl_vertex_usage);
	assert(ogl_is_succeed());
	::glBindBuffer(GL_ARRAY_BUFFER, 0);
	assert(ogl_is_succeed());


	// Index buffer.
	//
	if (param.has_index_)
	{
		const auto ogl_index_usage = usage_flags_to_ogl_usage(param.index_usage_flags_);
		const auto index_buffer_size = index_element_size * param.index_count_;

		::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ogl_index_buffer_);
		assert(ogl_is_succeed());
		::glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_buffer_size, param.raw_index_data_, ogl_index_usage);
		assert(ogl_is_succeed());
		::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		assert(ogl_is_succeed());
	}


	// Record the state.
	//
	vertex_format_ = param.vertex_format_;

	const auto float_size = static_cast<int>(sizeof(float));

	auto component_offset_ptr = static_cast<const char*>(nullptr);

	if (ogl_renderer_.current_vao_ != this)
	{
		ogl_renderer_.current_vao_ = nullptr;
	}

	::glBindVertexArray(ogl_vao_);
	assert(ogl_is_succeed());
	::glBindBuffer(GL_ARRAY_BUFFER, ogl_vertex_buffer_);
	assert(ogl_is_succeed());

	if (param.has_index_)
	{
		::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ogl_index_buffer_);
		assert(ogl_is_succeed());
	}

	// Position
	//
	{
		const auto position_item_count = 3 + (vertex_format_.is_position_transformed_);
		const auto position_size = position_item_count * float_size;

		::glEnableVertexAttribArray(0);
		assert(ogl_is_succeed());
		::glVertexAttribPointer(0, position_item_count, GL_FLOAT, GL_FALSE, vertex_size, component_offset_ptr);
		assert(ogl_is_succeed());

		component_offset_ptr += position_size;
	}


	// Blending weights.
	//
	if (vertex_format_.has_blending_weights())
	{
		const auto blending_weights_item_count = vertex_format_.blending_weight_count_;
		const auto blending_weights_size = blending_weights_item_count * float_size;

		::glEnableVertexAttribArray(1);
		assert(ogl_is_succeed());
		::glVertexAttribPointer(1, blending_weights_item_count, GL_FLOAT, GL_FALSE, vertex_size, component_offset_ptr);
		assert(ogl_is_succeed());

		component_offset_ptr += blending_weights_size;
	}

	// Normal.
	//
	if (vertex_format_.has_normal_)
	{
		const auto normal_item_count = 3;
		const auto normals_size = normal_item_count * float_size;

		::glEnableVertexAttribArray(2);
		assert(ogl_is_succeed());
		::glVertexAttribPointer(2, normal_item_count, GL_FLOAT, GL_FALSE, vertex_size, component_offset_ptr);
		assert(ogl_is_succeed());

		component_offset_ptr += normals_size;
	}

	if (vertex_format_.has_diffuse_)
	{
		const auto diffuse_size = float_size;

		::glEnableVertexAttribArray(3);
		assert(ogl_is_succeed());
		::glVertexAttribPointer(3, GL_BGRA, GL_UNSIGNED_BYTE, GL_TRUE, vertex_size, component_offset_ptr);
		assert(ogl_is_succeed());

		component_offset_ptr += diffuse_size;
	}

	if (vertex_format_.has_tex_coord_sets())
	{
		auto base_array_index = 4;

		for (auto i = 0; i < vertex_format_.tex_coord_set_count_; ++i)
		{
			const auto array_index = base_array_index + i;
			const auto tex_coord_set_item_count = vertex_format_.tex_coord_item_counts_[i];
			const auto tex_coord_set_size = tex_coord_set_item_count * float_size;

			::glEnableVertexAttribArray(array_index);
			assert(ogl_is_succeed());
			::glVertexAttribPointer(array_index, tex_coord_set_item_count, GL_FLOAT, GL_FALSE, vertex_size, component_offset_ptr);
			assert(ogl_is_succeed());

			component_offset_ptr += tex_coord_set_size;
		}
	}

	::glBindVertexArray(0);
	assert(ogl_is_succeed());
	::glBindBuffer(GL_ARRAY_BUFFER, 0);
	assert(ogl_is_succeed());
	::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	assert(ogl_is_succeed());

	if (!ogl_is_succeed())
	{
		return false;
	}


	// Finalize.
	//
	is_initialized_ = true;

	vertex_usage_flags_ = param.vertex_usage_flags_;
	index_usage_flags_ = param.index_usage_flags_;
	vertex_count_ = param.vertex_count_;
	index_count_ = param.index_count_;

	return true;
}

void OglRendererImpl::VertexArrayObjectImpl::do_uninitialize()
{
	uninitialize_internal();
}

void OglRendererImpl::VertexArrayObjectImpl::do_set_vertex_data(
	const int vertex_index,
	const int vertex_count,
	const void* const raw_data)
{
	if (!is_initialized_ || vertex_index < 0 || vertex_count <= 0 || !raw_data)
	{
		return;
	}

	if ((vertex_index + vertex_count) > vertex_count_)
	{
		assert(!"Vertex window out of range.");
		return;
	}

	const auto data_offset = vertex_index * vertex_format_.vertex_size_;
	const auto data_size = vertex_count * vertex_format_.vertex_size_;

	::glBindBuffer(GL_ARRAY_BUFFER, ogl_vertex_buffer_);
	assert(ogl_is_succeed());
	::glBufferSubData(GL_ARRAY_BUFFER, data_offset, data_size, raw_data);
	assert(ogl_is_succeed());
	::glBindBuffer(GL_ARRAY_BUFFER, 0);
	assert(ogl_is_succeed());
}

void OglRendererImpl::VertexArrayObjectImpl::do_draw(
	const PrimitiveType primitive_type,
	const int index_base,
	const int vertex_base,
	const int primitive_count)
{
	if (!is_initialized_ || index_base < 0 || vertex_base < 0 || primitive_count <= 0)
	{
		return;
	}

	ogl_renderer_.set_has_diffuse(vertex_format_.has_diffuse_);
	ogl_renderer_.set_is_position_transformed(vertex_format_.is_position_transformed_);
	ogl_renderer_.apply_changes();

	if (ogl_renderer_.current_vao_ != this)
	{
		ogl_renderer_.current_vao_ = this;

		::glBindVertexArray(ogl_vao_);
		assert(ogl_is_succeed());
	}

	const auto vertex_count = calculate_vertex_count(primitive_type, primitive_count);
	const auto ogl_primitive_type = get_ogl_primitive_type(primitive_type);

	if (vertex_count <= 0 || ogl_primitive_type == GL_NONE)
	{
		return;
	}

	if (index_count_ > 0)
	{
		::glDrawElementsBaseVertex(
			ogl_primitive_type,
			vertex_count,
			GL_UNSIGNED_SHORT,
			reinterpret_cast<const char*>(static_cast<std::intptr_t>(index_base)),
			vertex_base);

		assert(ogl_is_succeed());
	}
	else
	{
		::glDrawArrays(ogl_primitive_type, vertex_base, vertex_count);
		assert(ogl_is_succeed());
	}
}

void OglRendererImpl::VertexArrayObjectImpl::uninitialize_internal()
{
	is_initialized_ = false;
	vertex_format_ = {};
	vertex_usage_flags_ = UsageFlags::none;
	index_usage_flags_ = UsageFlags::none;
	vertex_count_ = 0;
	index_count_ = 0;

	if (ogl_vao_)
	{
		if (ogl_renderer_.current_vao_ == this)
		{
			ogl_renderer_.current_vao_ = nullptr;

			::glBindVertexArray(0);
			assert(ogl_is_succeed());
		}

		::glDeleteVertexArrays(1, &ogl_vao_);
		assert(ogl_is_succeed());

		ogl_vao_ = 0;
	}

	if (ogl_vertex_buffer_)
	{
		::glDeleteBuffers(1, &ogl_vertex_buffer_);
		assert(ogl_is_succeed());

		ogl_vertex_buffer_ = 0;
	}

	if (ogl_index_buffer_)
	{
		::glDeleteBuffers(1, &ogl_index_buffer_);
		assert(ogl_is_succeed());

		ogl_index_buffer_ = 0;
	}
}

//
// OglRendererImpl::VertexArrayObjectImpl
// ==========================================================================


// ==========================================================================
// OglRenderer::VertexArrayObject::InitializeParam
//

OglRenderer::VertexArrayObject::InitializeParam::InitializeParam()
	:
	has_index_{},
	vertex_format_{},
	vertex_usage_flags_{},
	vertex_count_{},
	raw_vertex_data_{},
	index_usage_flags_{},
	index_count_{},
	raw_index_data_{}
{
}

bool OglRenderer::VertexArrayObject::InitializeParam::is_valid() const
{
	if (!vertex_format_.is_valid())
	{
		return false;
	}

	const auto ogl_vertex_usage = usage_flags_to_ogl_usage(vertex_usage_flags_);

	if (!ogl_vertex_usage || vertex_count_ <= 0)
	{
		return false;
	}

	if (has_index_)
	{
		const auto ogl_index_usage = usage_flags_to_ogl_usage(index_usage_flags_);

		if (!ogl_index_usage || index_count_ <= 0 || !raw_index_data_)
		{
			return false;
		}
	}

	return true;
}

//
// OglRenderer::VertexArrayObject::InitializeParam
// ==========================================================================


// ==========================================================================
// OglRendererImpl::VertexArrayObject
//

OglRenderer::VertexArrayObject::VertexArrayObject()
{
}

OglRenderer::VertexArrayObject::~VertexArrayObject()
{
}

bool OglRendererImpl::VertexArrayObject::initialize(
	const InitializeParam& param)
{
	return do_initialize(param);
}

void OglRendererImpl::VertexArrayObject::uninitialize()
{
	return do_uninitialize();
}

void OglRendererImpl::VertexArrayObject::set_vertex_data(
	const int vertex_count,
	const void* const raw_data)
{
	do_set_vertex_data(0, vertex_count, raw_data);
}

void OglRendererImpl::VertexArrayObject::set_vertex_data(
	const int vertex_index,
	const int vertex_count,
	const void* const raw_data)
{
	do_set_vertex_data(vertex_index, vertex_count, raw_data);
}

void OglRendererImpl::VertexArrayObject::draw(
	const PrimitiveType primitive_type,
	const int primitive_count)
{
	do_draw(primitive_type, 0, 0, primitive_count);
}

void OglRendererImpl::VertexArrayObject::draw(
	const PrimitiveType primitive_type,
	const int vertex_base,
	const int primitive_count)
{
	do_draw(primitive_type, 0, vertex_base, primitive_count);
}

void OglRendererImpl::VertexArrayObject::draw(
	const PrimitiveType primitive_type,
	const int index_base,
	const int vertex_base,
	const int primitive_count)
{
	do_draw(primitive_type, index_base, vertex_base, primitive_count);
}

//
// OglRendererImpl::VertexArrayObject
// ==========================================================================


// ==========================================================================
// OglRendererImpl::SamplerStateImpl::InitializeParam
//

OglRendererImpl::SamplerStateImpl::InitializeParam::InitializeParam()
	:
	unit_index_{-1},
	max_lod_bias_{},
	has_anisotropy_{},
	max_anisotropy_{}
{
}

bool OglRendererImpl::SamplerStateImpl::InitializeParam::is_valid() const
{
	if (unit_index_ < 0 || unit_index_ >= max_samplers)
	{
		assert(!"Texture unit index out of range.");
		return false;
	}

	if (max_lod_bias_ < 0.0F)
	{
		assert(!"Negative max absolute LOD bias.");
		return false;
	}

	if (has_anisotropy_ && max_anisotropy_ < 0.0F)
	{
		assert(!"Negative max anisotropy.");
		return false;
	}

	return true;
}

//
// OglRendererImpl::SamplerStateImpl::InitializeParam
// ==========================================================================


// ==========================================================================
// OglRendererImpl::SamplerStateImpl
//

OglRendererImpl::SamplerStateImpl::SamplerStateImpl()
	:
	is_initialized_{},
	unit_index_{},
	ogl_sampler_{},
	addressing_mode_u_{},
	addressing_mode_v_{},
	mag_filter_{},
	min_filter_{},
	mip_filter_{},
	lod_bias_{},
	max_lod_bias_{},
	has_anisotropy_{},
	anisotropy_{},
	max_anisotropy_{}
{
}

OglRendererImpl::SamplerStateImpl::~SamplerStateImpl()
{
	assert(!is_initialized_);
}

bool OglRendererImpl::SamplerStateImpl::initialize(
	const InitializeParam& param)
{
	uninitialize();

	if (!param.is_valid())
	{
		return false;
	}

	unit_index_ = param.unit_index_;
	max_lod_bias_ = param.max_lod_bias_;
	has_anisotropy_ = param.has_anisotropy_;
	max_anisotropy_ = param.max_anisotropy_;

	::glGenSamplers(1, &ogl_sampler_);
	assert(ogl_is_succeed());

	::glBindSampler(unit_index_, ogl_sampler_);
	assert(ogl_is_succeed());

	if (!ogl_is_succeed())
	{
		return false;
	}

	set_defaults();

	return true;
}

void OglRendererImpl::SamplerStateImpl::uninitialize()
{
	is_initialized_ = false;

	if (unit_index_ >= 0)
	{
		::glBindSampler(unit_index_, 0);
		assert(ogl_is_succeed());

		unit_index_ = -1;
	}

	if (ogl_sampler_)
	{
		::glDeleteSamplers(1, &ogl_sampler_);
		assert(ogl_is_succeed());

		ogl_sampler_ = 0;
	}

	addressing_mode_u_ = TextureAddressingMode::none;
	addressing_mode_v_ = TextureAddressingMode::none;
	mag_filter_ = TextureFilterType::none;
	min_filter_ = TextureFilterType::none;
	mip_filter_ = TextureFilterType::none;
	lod_bias_ = 0.0F;
	max_lod_bias_ = 0.0F;
	has_anisotropy_ = false;
	anisotropy_ = 0;
	max_anisotropy_ = 0;
}

OglRendererImpl::TextureAddressingMode OglRendererImpl::SamplerStateImpl::do_get_addressing_mode_u() const
{
	assert(is_initialized_);
	return addressing_mode_u_;
}

void OglRendererImpl::SamplerStateImpl::do_set_addressing_mode_u(
	const TextureAddressingMode addressing_mode_u)
{
	switch (addressing_mode_u)
	{
	case TextureAddressingMode::clamp:
	case TextureAddressingMode::wrap:
		break;

	default:
		assert(!"Unsupported texture addressing mode.");
		return;
	}

	addressing_mode_u_ = addressing_mode_u;
	set_addressing_mode_u_internal();
}

OglRendererImpl::TextureAddressingMode OglRendererImpl::SamplerStateImpl::do_get_addressing_mode_v() const
{
	assert(is_initialized_);
	return addressing_mode_v_;
}

void OglRendererImpl::SamplerStateImpl::do_set_addressing_mode_v(
	const TextureAddressingMode addressing_mode_v)
{
	switch (addressing_mode_v)
	{
	case TextureAddressingMode::clamp:
	case TextureAddressingMode::wrap:
		break;

	default:
		assert(!"Unsupported texture addressing mode.");
		return;
	}

	addressing_mode_v_ = addressing_mode_v;
	set_addressing_mode_v_internal();
}

OglRendererImpl::TextureFilterType OglRendererImpl::SamplerStateImpl::do_get_mag_filter() const
{
	assert(is_initialized_);
	return mag_filter_;
}

void OglRendererImpl::SamplerStateImpl::do_set_mag_filter(
	const TextureFilterType mag_filter)
{
	if (mag_filter_ == mag_filter)
	{
		return;
	}

	mag_filter_ = mag_filter;
	set_mag_filter_internal();
}

OglRendererImpl::TextureFilterType OglRendererImpl::SamplerStateImpl::do_get_min_filter() const
{
	assert(is_initialized_);
	return min_filter_;
}

void OglRendererImpl::SamplerStateImpl::do_set_min_filter(
	const TextureFilterType min_filter)
{
	if (min_filter_ == min_filter)
	{
		return;
	}

	min_filter_ = min_filter;
	set_min_filter_internal();
}

OglRendererImpl::TextureFilterType OglRendererImpl::SamplerStateImpl::do_get_mip_filter() const
{
	assert(is_initialized_);
	return mip_filter_;
}

void OglRendererImpl::SamplerStateImpl::do_set_mip_filter(
	const TextureFilterType mip_filter)
{
	if (mip_filter_ == mip_filter)
	{
		return;
	}

	mip_filter_ = mip_filter;
	set_mip_filter_internal();
}

float OglRendererImpl::SamplerStateImpl::do_get_lod_bias() const
{
	assert(is_initialized_);
	return lod_bias_;
}

void OglRendererImpl::SamplerStateImpl::do_set_lod_bias(
	const float lod_bias)
{
	if (lod_bias_ == lod_bias)
	{
		return;
	}

	lod_bias_ = lod_bias;
	set_lod_bias_internal();
}

float OglRendererImpl::SamplerStateImpl::do_get_anisotropy() const
{
	assert(is_initialized_);
	return anisotropy_;
}

void OglRendererImpl::SamplerStateImpl::do_set_anisotropy(
	const float anisotropy)
{
	if (anisotropy < 0.0F)
	{
		assert(!"Negative max anisotropy.");
		return;
	}

	if (anisotropy_ == anisotropy)
	{
		return;
	}

	anisotropy_ = anisotropy;
	set_anisotropy_internal();
}

void OglRendererImpl::SamplerStateImpl::set_addressing_mode_u_internal()
{
	const auto ogl_addressing_mode_u = get_ogl_addressing_mode(addressing_mode_u_);
	::glSamplerParameteri(ogl_sampler_, GL_TEXTURE_WRAP_S, ogl_addressing_mode_u);
	assert(ogl_is_succeed());
}

void OglRendererImpl::SamplerStateImpl::set_addressing_mode_v_internal()
{
	const auto ogl_addressing_mode_v = get_ogl_addressing_mode(addressing_mode_v_);
	::glSamplerParameteri(ogl_sampler_, GL_TEXTURE_WRAP_T, ogl_addressing_mode_v);
	assert(ogl_is_succeed());
}

void OglRendererImpl::SamplerStateImpl::set_mag_filter_internal()
{
	const auto mag_filter = get_ogl_mag_filter(mag_filter_);
	::glSamplerParameteri(ogl_sampler_, GL_TEXTURE_MAG_FILTER, mag_filter);
	assert(ogl_is_succeed());
}

void OglRendererImpl::SamplerStateImpl::set_minmip_filter_internal()
{
	const auto min_filter = get_ogl_min_filter(min_filter_, mip_filter_);
	::glSamplerParameteri(ogl_sampler_, GL_TEXTURE_MIN_FILTER, min_filter);
	assert(ogl_is_succeed());
}

void OglRendererImpl::SamplerStateImpl::set_min_filter_internal()
{
	set_minmip_filter_internal();
}

void OglRendererImpl::SamplerStateImpl::set_mip_filter_internal()
{
	set_minmip_filter_internal();
}

void OglRendererImpl::SamplerStateImpl::set_lod_bias_internal()
{
	auto lod_bias = lod_bias_;

	if (lod_bias < (-max_lod_bias_))
	{
		lod_bias = -max_lod_bias_;
	}
	else if (lod_bias > max_lod_bias_)
	{
		lod_bias = max_lod_bias_;
	}

	::glSamplerParameterf(ogl_sampler_, GL_TEXTURE_LOD_BIAS, lod_bias);
	assert(ogl_is_succeed());
}

void OglRendererImpl::SamplerStateImpl::set_anisotropy_internal()
{
	if (!has_anisotropy_)
	{
		return;
	}

	auto anisotropy = anisotropy_;

	if (anisotropy < min_anisotropy)
	{
		anisotropy = min_anisotropy;
	}

	if (anisotropy > max_anisotropy_)
	{
		anisotropy = max_anisotropy_;
	}

	::glSamplerParameterf(ogl_sampler_, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy);
	assert(ogl_is_succeed());
}

void OglRendererImpl::SamplerStateImpl::set_defaults()
{
	addressing_mode_u_ = default_addressing_mode_u;
	set_addressing_mode_u_internal();

	addressing_mode_v_ = default_addressing_mode_v;
	set_addressing_mode_v_internal();

	mag_filter_ = default_mag_filter;
	set_mag_filter_internal();

	min_filter_ = default_min_filter;
	mip_filter_ = default_mip_filter;
	set_minmip_filter_internal();

	lod_bias_ = default_lod_bias;
	set_lod_bias_internal();

	anisotropy_ = default_max_anisotropy;
	set_anisotropy_internal();
}

GLenum OglRendererImpl::SamplerStateImpl::get_ogl_addressing_mode(
	const TextureAddressingMode d3d_addressing_mode)
{
	switch (d3d_addressing_mode)
	{
	case OglRenderer::TextureAddressingMode::clamp:
		return GL_CLAMP_TO_EDGE;

	case OglRenderer::TextureAddressingMode::wrap:
		return GL_REPEAT;

	default:
		assert(!"Unsupported texture addressing mode.");
		return invalid_ogl_enum;
	}
}

GLenum OglRendererImpl::SamplerStateImpl::get_ogl_mag_filter(
	const TextureFilterType d3d_mag_filter)
{
	switch (d3d_mag_filter)
	{
	case TextureFilterType::point:
		return GL_NEAREST;

	case TextureFilterType::linear:
	case TextureFilterType::anisotropic:
		return GL_LINEAR;

	default:
		assert(!"Unsupported magnification texture filter.");
		return invalid_ogl_enum;
	}
}

GLenum OglRendererImpl::SamplerStateImpl::get_ogl_min_filter(
	const TextureFilterType d3d_min_filter,
	const TextureFilterType d3d_mip_filter)
{
	switch (d3d_min_filter)
	{
	case TextureFilterType::point:
		switch (d3d_mip_filter)
		{
		case TextureFilterType::none:
			return GL_NEAREST;

		case TextureFilterType::point:
			return GL_NEAREST_MIPMAP_NEAREST;

		default:
			assert(!"Unsupported mipmap texture filter.");
			return invalid_ogl_enum;
		}
		break;

	case TextureFilterType::linear:
		switch (d3d_mip_filter)
		{
		case TextureFilterType::none:
			return GL_LINEAR;

		case TextureFilterType::point:
			return GL_LINEAR_MIPMAP_NEAREST;

		default:
			assert(!"Unsupported mipmap texture filter.");
			return invalid_ogl_enum;
		}
		break;

	case TextureFilterType::anisotropic:
		switch (d3d_mip_filter)
		{
		case TextureFilterType::none:
			return GL_LINEAR;

		case TextureFilterType::point:
			return GL_LINEAR_MIPMAP_LINEAR;

		default:
			assert(!"Unsupported mipmap texture filter.");
			return invalid_ogl_enum;
		}
		break;

	default:
		assert(!"Unsupported minification texture filter.");
		return invalid_ogl_enum;
	}
}

//
// OglRendererImpl::SamplerStateImpl
// ==========================================================================


// ==========================================================================
// OglRenderer::SamplerState
//

OglRenderer::SamplerState::SamplerState()
{
}

OglRenderer::SamplerState::~SamplerState()
{
}

OglRenderer::TextureAddressingMode OglRenderer::SamplerState::get_addressing_mode_u() const
{
	return do_get_addressing_mode_u();
}

void OglRenderer::SamplerState::set_addressing_mode_u(
	const TextureAddressingMode addressing_mode_u)
{
	do_set_addressing_mode_u(addressing_mode_u);
}

OglRenderer::TextureAddressingMode OglRenderer::SamplerState::get_addressing_mode_v() const
{
	return do_get_addressing_mode_v();
}

void OglRenderer::SamplerState::set_addressing_mode_v(
	const TextureAddressingMode addressing_mode_v)
{
	do_set_addressing_mode_v(addressing_mode_v);
}

OglRenderer::TextureFilterType OglRenderer::SamplerState::get_mag_filter() const
{
	return do_get_mag_filter();
}

void OglRenderer::SamplerState::set_mag_filter(
	const TextureFilterType mag_filter)
{
	do_set_mag_filter(mag_filter);
}

OglRenderer::TextureFilterType OglRenderer::SamplerState::get_min_filter() const
{
	return do_get_min_filter();
}

void OglRenderer::SamplerState::set_min_filter(
	const TextureFilterType min_filter)
{
	do_set_min_filter(min_filter);
}

OglRenderer::TextureFilterType OglRenderer::SamplerState::get_mip_filter() const
{
	return do_get_mip_filter();
}

void OglRenderer::SamplerState::set_mip_filter(
	const TextureFilterType mip_filter)
{
	do_set_mip_filter(mip_filter);
}

float OglRenderer::SamplerState::get_lod_bias() const
{
	return do_get_lod_bias();
}

void OglRenderer::SamplerState::set_lod_bias(
	const float mipmap_lod_bias)
{
	do_set_lod_bias(mipmap_lod_bias);
}

float OglRenderer::SamplerState::get_anisotropy() const
{
	return do_get_anisotropy();
}

void OglRenderer::SamplerState::set_anisotropy(
	const float anisotropy)
{
	do_set_anisotropy(anisotropy);
}

//
// OglRenderer::SamplerState
// ==========================================================================


// ==========================================================================
// OglRenderer::Viewport
//

OglRenderer::Viewport::Viewport()
	:
	x_{},
	y_{},
	width_{},
	height_{},
	depth_min_z_{},
	depth_max_z_{}
{
}

//
// OglRenderer::Viewport
// ==========================================================================


// ==========================================================================
// OglRenderer::Fvf
//

OglRenderer::Fvf::Fvf()
	:
	has_position_{},
	is_position_transformed_{},
	blending_weight_count_{},
	has_normal_{},
	has_diffuse_{},
	tex_coord_set_count_{},
	tex_coord_item_counts_{},
	vertex_size_{}
{
}

OglRenderer::Fvf::Fvf(
	const std::uint32_t d3d_fvf)
{
	*this = from_d3d(d3d_fvf);
}

bool OglRenderer::Fvf::has_blending_weights() const
{
	return blending_weight_count_ > 0;
}

bool OglRenderer::Fvf::has_tex_coord_sets() const
{
	return tex_coord_set_count_ > 0;
}

bool OglRenderer::Fvf::is_valid() const
{
	return has_position_ && vertex_size_ > 0;
}

OglRenderer::Fvf OglRenderer::Fvf::from_d3d(
	const std::uint32_t d3d_fvf)
{
	auto is_zero = false;

	if (d3d_fvf == 0)
	{
		is_zero = true;
	}

	if ((d3d_fvf & d3dfvf_valid_flags) != d3d_fvf)
	{
		assert(!"Unsupported flags.");
		is_zero = true;
	}

	if (is_zero)
	{
		return {};
	}

	auto has_transformed = false;
	auto has_untransformed = false;

	if (((d3d_fvf & d3dfvf_xyz) == d3dfvf_xyz && (d3d_fvf & d3dfvf_xyzrhw) != d3dfvf_xyzrhw) ||
		(d3d_fvf & d3dfvf_xyzb1) == d3dfvf_xyzb1 ||
		(d3d_fvf & d3dfvf_xyzb2) == d3dfvf_xyzb2 ||
		(d3d_fvf & d3dfvf_xyzb3) == d3dfvf_xyzb3)
	{
		has_untransformed = true;
	}

	if ((d3d_fvf & d3dfvf_xyzrhw) == d3dfvf_xyzrhw)
	{
		has_transformed = true;
	}

	if (has_transformed && has_untransformed)
	{
		assert(!"Transformed and untransformed are mutual exclusive.");
		return {};
	}

	auto has_normal = false;

	if ((d3d_fvf & d3dfvf_normal) == d3dfvf_normal)
	{
		has_normal = true;
	}

	if (has_transformed && has_normal)
	{
		assert(!"Transformed and normal are prohibited.");
		return {};
	}


	auto fvf = Fvf{};

	if (has_transformed)
	{
		fvf.has_position_ = true;
		fvf.is_position_transformed_ = true;
	}

	if (has_untransformed)
	{
		fvf.has_position_ = true;
		fvf.is_position_transformed_ = false;
	}

	if ((d3d_fvf & d3dfvf_xyzb1) == d3dfvf_xyzb1)
	{
		fvf.has_position_ = true;
		fvf.blending_weight_count_ = 1;
	}

	if ((d3d_fvf & d3dfvf_xyzb2) == d3dfvf_xyzb2)
	{
		fvf.has_position_ = true;
		fvf.blending_weight_count_ = 2;
	}

	if ((d3d_fvf & d3dfvf_xyzb3) == d3dfvf_xyzb3)
	{
		fvf.has_position_ = true;
		fvf.blending_weight_count_ = 3;
	}

	if ((d3d_fvf & d3dfvf_normal) == d3dfvf_normal)
	{
		fvf.has_normal_ = true;
	}

	if ((d3d_fvf & d3dfvf_diffuse) == d3dfvf_diffuse)
	{
		fvf.has_diffuse_ = true;
	}

	if ((d3d_fvf & d3dfvf_tex1) == d3dfvf_tex1)
	{
		fvf.tex_coord_set_count_ = 1;
	}

	if ((d3d_fvf & d3dfvf_tex2) == d3dfvf_tex2)
	{
		fvf.tex_coord_set_count_ = 2;
	}

	if ((d3d_fvf & d3dfvf_tex3) == d3dfvf_tex3)
	{
		fvf.tex_coord_set_count_ = 3;
	}

	if ((d3d_fvf & d3dfvf_tex4) == d3dfvf_tex4)
	{
		fvf.tex_coord_set_count_ = 4;
	}

	for (auto i = 0; i < fvf.tex_coord_set_count_; ++i)
	{
		if ((d3d_fvf & d3dfvf_texcoordsize4(i)) == d3dfvf_texcoordsize4(i))
		{
			fvf.tex_coord_item_counts_[i] = 4;
		}
		else if ((d3d_fvf & d3dfvf_texcoordsize3(i)) == d3dfvf_texcoordsize3(i))
		{
			fvf.tex_coord_item_counts_[i] = 3;
		}
		else
		{
			fvf.tex_coord_item_counts_[i] = 2;
		}
	}


	// Calculate a size of the vertex.
	// Each component item has four byte size (float, DWORD).
	//
	fvf.vertex_size_ = 4 * (
		(fvf.has_position_ ? (fvf.is_position_transformed_ ? 4 : 3) : 0) +
		fvf.blending_weight_count_ +
		(fvf.has_normal_ ? 3 : 0) +
		(fvf.has_diffuse_ ? 1 : 0) +
		std::accumulate(
			fvf.tex_coord_item_counts_.cbegin(),
			fvf.tex_coord_item_counts_.cbegin() + fvf.tex_coord_set_count_,
			0) +
		0);

	return fvf;
}

//
// OglRenderer::Fvf
// ==========================================================================


// ==========================================================================
// OglRendererImpl::TextureImpl
//

OglRendererImpl::TextureImpl::TextureImpl(
	OglRendererImpl& ogl_renderer)
	:
	ogl_renderer_{ogl_renderer},
	is_initialized_{},
	ogl_texture_{},
	type_{},
	is_compressed_{},
	surface_format_{},
	width_{},
	height_{},
	level_count_{},
	max_level_count_{},
	buffer_{}
{
}

OglRendererImpl::TextureImpl::~TextureImpl()
{
	uninitialize_internal();
}

bool OglRendererImpl::TextureImpl::do_initialize(
	const InitializeParam& param)
{
	uninitialize_internal();

	auto old_is_current_context = false;

	auto guard_context = ul::ScopeGuard{
		[&]()
		{
			old_is_current_context = ogl_renderer_.set_post_is_current_context(true);
		},

		[&]()
		{
			ogl_renderer_.set_is_current_context(old_is_current_context);
		},
	};

	if (!initialize_internal(param))
	{
		uninitialize_internal();
		return false;
	}

	return true;
}

void OglRendererImpl::TextureImpl::do_uninitialize()
{
	uninitialize_internal();
}

bool OglRendererImpl::TextureImpl::do_upload_level(
	const UploadParam& param)
{
	if (!param.is_valid())
	{
		return false;
	}

	const auto level_count = (level_count_ > 0 ? level_count_ : max_level_count_);

	if (param.level_ >= level_count)
	{
		assert(!"Level out of range.");
		return false;
	}

	if (type_ == Type::cube_map)
	{
		if (param.cube_face_index_ < 0 || param.cube_face_index_ >= 6)
		{
			assert(!"Cube face index out of range.");
			return false;
		}
	}

	auto old_is_current_context = false;

	auto guard_context = ul::ScopeGuard{
		[&]()
		{
			old_is_current_context = ogl_renderer_.set_post_is_current_context(true);
		},

		[&]()
		{
			ogl_renderer_.set_is_current_context(old_is_current_context);
		},
	};

	if (param.has_linear_filter_)
	{
		if (param.src_surface_format_ == SurfaceFormat::a4r4g4b4 && surface_format_ == SurfaceFormat::a4r4g4b4)
		{
			return upload_level_a4r4g4b4_linear(param);
		}
		else if (param.src_surface_format_ == SurfaceFormat::a8r8g8b8 && surface_format_ == SurfaceFormat::v8u8)
		{
			return upload_level_a8r8g8b8_to_v8u8_linear(param);
		}
		else
		{
			assert(!"Unsupported parameters combination.");
			return false;
		}
	}
	else
	{
		if (param.src_surface_format_ == surface_format_)
		{
			return upload_level_no_filter(param, param.raw_data_);
		}
		else
		{
			assert(!"Unsupported parameters combination.");
			return false;
		}
	}
}

OglRenderer::Texture::Type OglRendererImpl::TextureImpl::do_get_type() const
{
	assert(is_initialized_);
	return type_;
}

OglRenderer::SurfaceFormat OglRendererImpl::TextureImpl::do_get_surface_format() const
{
	assert(is_initialized_);
	return surface_format_;
}

bool OglRendererImpl::TextureImpl::do_is_compressed() const
{
	assert(is_initialized_);
	return is_compressed_;
}

int OglRendererImpl::TextureImpl::do_get_width() const
{
	assert(is_initialized_);
	return width_;
}

int OglRendererImpl::TextureImpl::do_get_height() const
{
	assert(is_initialized_);
	return height_;
}

int OglRendererImpl::TextureImpl::do_get_level_count() const
{
	assert(is_initialized_);
	return level_count_;
}

int OglRendererImpl::TextureImpl::do_get_level_width(
	const int level) const
{
	assert(is_initialized_);
	assert(level >= 0 && level < (level_count_ > 0 ? level_count_ : max_level_count_));
	return width_ / (1 << level);
}

int OglRendererImpl::TextureImpl::do_get_level_height(
	const int level) const
{
	assert(is_initialized_);
	assert(level >= 0 && level < (level_count_ > 0 ? level_count_ : max_level_count_));
	return height_ / (1 << level);
}

bool OglRendererImpl::TextureImpl::initialize_internal(
	const InitializeParam& param)
{
	if (!param.is_valid())
	{
		return false;
	}

	type_ = param.type_;

	const auto is_2d = (type_ == Type::two_d);

	surface_format_ = param.surface_format_;

	switch (surface_format_)
	{
	case SurfaceFormat::a4r4g4b4:
	case SurfaceFormat::a8r8g8b8:
	case SurfaceFormat::v8u8:
		is_compressed_ = false;
		break;

	case SurfaceFormat::dxt1:
	case SurfaceFormat::dxt3:
	case SurfaceFormat::dxt5:
		if (!ogl_renderer_.has_gl_ext_texture_compression_s3tc_)
		{
			assert(!"DXTC not supported.");
			return false;
		}

		is_compressed_ = true;
		break;

	default:
		assert(!"Unsupported surface format.");
		return false;
	}

	width_ = param.width_;
	height_ = param.height_;

	if (width_ <= 0 || height_ <= 0)
	{
		assert(!"Unsupported texture size.");
		return false;
	}

	level_count_ = param.level_count_;

	if (level_count_ < 0)
	{
		assert(!"Negative level count.");
		return false;
	}

	max_level_count_ = calculate_max_levels(width_, height_);

	if (level_count_ > max_level_count_)
	{
		assert(!"Level count out of range.");
		return false;
	}

	::glGenTextures(1, &ogl_texture_);
	assert(ogl_is_succeed());

	if (!ogl_is_succeed())
	{
		return false;
	}

	if (is_2d)
	{
		if (!initialize_2d_internal())
		{
			return false;
		}
	}
	else
	{
		if (!initialize_cube_map_internal())
		{
			return false;
		}
	}

	is_initialized_ = true;

	return true;
}

bool OglRendererImpl::TextureImpl::initialize_2d_internal()
{
	if (ogl_renderer_.current_2d_texture_ != this)
	{
		::glBindTexture(GL_TEXTURE_2D, ogl_texture_);
		assert(ogl_is_succeed());

		ogl_renderer_.current_2d_texture_ = this;
	}


	auto level_count = level_count_;

	if (level_count == 0)
	{
		level_count = max_level_count_;
	}

	::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, level_count - 1);
	assert(ogl_is_succeed());

	::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	assert(ogl_is_succeed());
	::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	assert(ogl_is_succeed());

	if (level_count_ > 1)
	{
		::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
		assert(ogl_is_succeed());
	}
	else
	{
		::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		assert(ogl_is_succeed());
	}

	::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	assert(ogl_is_succeed());

	const auto ogl_internal_format = get_ogl_internal_pixel_format();

	for (auto i = 0; i < level_count; ++i)
	{
		const auto level_width = width_ / (1 << i);
		const auto level_height = height_ / (1 << i);

		if (is_compressed_)
		{
			const auto image_size = calculate_compressed_image_size(level_width, level_height);

			::glCompressedTexImage2D(GL_TEXTURE_2D, i, ogl_internal_format, level_width, level_height, 0, image_size, nullptr);
			assert(ogl_is_succeed());
		}
		else
		{
			::glTexImage2D(GL_TEXTURE_2D, i, ogl_internal_format, level_width, level_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
			assert(ogl_is_succeed());
		}
	}

	return ogl_is_succeed();
}

bool OglRendererImpl::TextureImpl::initialize_cube_map_internal()
{
	if (width_ != height_)
	{
		assert(!"Expected square texture for cube map.");
		return false;
	}

	if (ogl_renderer_.current_cube_map_texture_ != this)
	{
		::glBindTexture(GL_TEXTURE_CUBE_MAP, ogl_texture_);
		assert(ogl_is_succeed());

		ogl_renderer_.current_cube_map_texture_ = this;
	}

	assert(!"Not implemented.");
	return false;
}

void OglRendererImpl::TextureImpl::uninitialize_internal()
{
	is_initialized_ = false;

	if (ogl_texture_)
	{
		auto is_current_2d = false;
		auto is_current_cube_map = false;

		GLenum ogl_target;

		switch (type_)
		{
		case Type::two_d:
			ogl_target = GL_TEXTURE_2D;
			is_current_2d = (ogl_renderer_.current_2d_texture_ == this);
			break;

		case Type::cube_map:
			ogl_target = GL_TEXTURE_CUBE_MAP;
			is_current_cube_map = (ogl_renderer_.current_cube_map_texture_ == this);
			break;

		default:
			assert(!"Unsupported texture target.");
			ogl_target = invalid_ogl_enum;
			break;
		}

		if (is_current_2d)
		{
			ogl_renderer_.current_2d_texture_ = nullptr;

			::glBindTexture(GL_TEXTURE_2D, 0);
			assert(ogl_is_succeed());
		}

		if (is_current_cube_map)
		{
			ogl_renderer_.current_cube_map_texture_ = nullptr;

			::glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
			assert(ogl_is_succeed());
		}

		::glDeleteTextures(1, &ogl_texture_);
		assert(ogl_is_succeed());

		ogl_texture_ = 0;
	}

	type_ = Type::none;
	is_compressed_ = false;
	surface_format_ = SurfaceFormat::none;
	width_ = 0;
	height_ = 0;
	level_count_ = 0;
	max_level_count_ = 0;
}

int OglRendererImpl::TextureImpl::calculate_max_levels(
	const int width,
	const int height)
{
	if (width <= 0 || height <= 0)
	{
		assert(!"Non-positive width or height.");
		return 0;
	}

	auto level_width = width;
	auto level_height = height;
	auto level_count = 0;

	while (!(level_width == 0 && level_height == 0))
	{
		level_count += 1;

		level_width /= 2;
		level_height /= 2;
	}

	return level_count;
}

int OglRendererImpl::TextureImpl::get_ogl_internal_pixel_format() const
{
	switch (surface_format_)
	{
	case SurfaceFormat::a4r4g4b4:
		return GL_RGBA4;

	case SurfaceFormat::a8r8g8b8:
		return GL_RGBA8;

	case SurfaceFormat::v8u8:
		return GL_RG8_SNORM;

	case SurfaceFormat::dxt1:
		if (!ogl_renderer_.has_gl_ext_texture_compression_s3tc_)
		{
			return invalid_ogl_format;
		}

		return GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;

	case SurfaceFormat::dxt3:
		if (!ogl_renderer_.has_gl_ext_texture_compression_s3tc_)
		{
			return invalid_ogl_format;
		}

		return GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;

	case SurfaceFormat::dxt5:
		if (!ogl_renderer_.has_gl_ext_texture_compression_s3tc_)
		{
			return invalid_ogl_format;
		}

		return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;

	default:
		assert(!"Unsupported surface format.");
		return invalid_ogl_format;
	}
}

void OglRendererImpl::TextureImpl::get_ogl_pixel_format_and_type(
	GLenum& format,
	GLenum& type) const
{
	format = invalid_ogl_enum;
	type = invalid_ogl_enum;

	switch (surface_format_)
	{
	case SurfaceFormat::a4r4g4b4:
		format = GL_BGRA;
		type = GL_UNSIGNED_SHORT_4_4_4_4;
		break;

	case SurfaceFormat::a8r8g8b8:
		format = GL_BGRA;
		type = GL_UNSIGNED_BYTE;
		break;

	case SurfaceFormat::v8u8:
		format = GL_RG;
		type = GL_UNSIGNED_BYTE;

	case SurfaceFormat::dxt1:
	case SurfaceFormat::dxt3:
	case SurfaceFormat::dxt5:
	default:
		assert(!"Unsupported surface format.");
		break;
	}
}

int OglRendererImpl::TextureImpl::calculate_compressed_image_size(
	const int width,
	const int height) const
{
	auto block_size = 0;

	switch (surface_format_)
	{
	case SurfaceFormat::dxt1:
		block_size = 8;
		break;

	case SurfaceFormat::dxt3:
	case SurfaceFormat::dxt5:
		block_size = 16;
		break;

	default:
		assert(!"Unsupported surface format.");
		return 0;
	}

	const auto row_width = ((width + 3) / 4);
	const auto row_height = ((height + 3) / 4);
	const auto image_size = block_size * row_width * row_height;

	return image_size;
}

bool OglRendererImpl::TextureImpl::upload_level_no_filter(
	const UploadParam& param,
	const void* raw_data)
{
	const auto is_cube_map = (type_ == Type::cube_map);
	const auto level_width = width_ / (1 << param.level_);
	const auto level_height = height_ / (1 << param.level_);

	const auto ogl_texture_target = GLenum(is_cube_map ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D);

	::glBindTexture(ogl_texture_target, ogl_texture_);
	assert(ogl_is_succeed());

	GLenum ogl_level_target;

	if (is_cube_map)
	{
		switch (param.cube_face_index_)
		{
		case 0:
			ogl_level_target = GL_TEXTURE_CUBE_MAP_POSITIVE_X;
			break;

		case 1:
			ogl_level_target = GL_TEXTURE_CUBE_MAP_NEGATIVE_X;
			break;

		case 2:
			ogl_level_target = GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
			break;

		case 3:
			ogl_level_target = GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
			break;

		case 4:
			ogl_level_target = GL_TEXTURE_CUBE_MAP_POSITIVE_Z;
			break;

		case 5:
			ogl_level_target = GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
			break;

		default:
			ogl_level_target = invalid_ogl_enum;
		}
	}
	else
	{
		ogl_level_target = GL_TEXTURE_2D;
	}

	if (is_compressed_)
	{
		const auto ogl_format = get_ogl_internal_pixel_format();
		const auto image_size = calculate_compressed_image_size(level_width, level_height);

		::glCompressedTexSubImage2D(
			ogl_level_target,
			param.level_,
			0,
			0,
			level_width,
			level_height,
			ogl_format,
			image_size,
			param.raw_data_);

		assert(ogl_is_succeed());
	}
	else
	{
		GLenum ogl_format;
		GLenum ogl_type;

		get_ogl_pixel_format_and_type(ogl_format, ogl_type);

		::glTexSubImage2D(
			ogl_level_target,
			param.level_,
			0,
			0,
			level_width,
			level_height,
			ogl_format,
			ogl_type,
			param.raw_data_);

		assert(ogl_is_succeed());
	}

	if (param.level_ == 0 && level_count_ == 0)
	{
		::glGenerateMipmap(ogl_texture_target);
		assert(ogl_is_succeed());
	}

	return true;
}

bool OglRendererImpl::TextureImpl::upload_level_a4r4g4b4_linear(
	const UploadParam& param)
{
	const auto bytes_per_pixel = 2;
	const auto level_width = width_ / (1 << param.level_);
	const auto level_height = height_ / (1 << param.level_);
	const auto pitch = bytes_per_pixel * level_width;
	const auto buffer_size = pitch * level_height;

	if (static_cast<int>(buffer_.size()) < buffer_size)
	{
		buffer_.resize(buffer_size);
	}

	auto dst_memory = buffer_.data();

	for (auto h = 0; h < level_height; ++h)
	{
		auto dst_row = reinterpret_cast<std::uint16_t*>(dst_memory);

		for (auto w = 0; w < level_width; ++w)
		{
			dst_row[w] = interpolate_pixel_linearly_a4r4g4b4(
				w,
				h,
				level_width,
				level_height,
				static_cast<const std::uint8_t*>(param.raw_data_));
		}

		dst_memory += pitch;
	}

	return upload_level_no_filter(param, buffer_.data());
}

bool OglRendererImpl::TextureImpl::upload_level_a8r8g8b8_to_v8u8_linear(
	const UploadParam& param)
{
	const auto dst_bytes_per_pixel = 2;
	const auto level_width = width_ / (1 << param.level_);
	const auto level_height = height_ / (1 << param.level_);
	const auto dst_pitch = dst_bytes_per_pixel * level_width;
	const auto buffer_size = dst_pitch * level_height;

	if (static_cast<int>(buffer_.size()) < buffer_size)
	{
		buffer_.resize(buffer_size);
	}

	auto dst_memory = buffer_.data();

	for (auto h = 0; h < level_height; ++h)
	{
		auto dst_row = reinterpret_cast<std::uint16_t*>(dst_memory);

		for (auto w = 0; w < level_width; ++w)
		{
			dst_row[w] = interpolate_pixel_linearly_a8r8g8b8_to_v8u8(
				w,
				h,
				level_width,
				level_height,
				static_cast<const std::uint8_t*>(param.raw_data_));
		}

		dst_memory += dst_pitch;
	}

	return upload_level_no_filter(param, buffer_.data());
}

std::uint16_t OglRendererImpl::TextureImpl::interpolate_pixel_linearly_a4r4g4b4(
	const int x,
	const int y,
	const int width,
	const int height,
	const std::uint8_t* src_memory)
{
	constexpr auto max_sample_count = 4;

	struct SampleDelta
	{
		int dx_;
		int dy_;
	}; // SampleDelta

	constexpr SampleDelta sample_deltas[max_sample_count] =
	{
		{0, -1}, {-1, 0}, {0, 1}, {1, 0},
	};

	const auto src_pitch = 2 * width;

	auto a_sum = 0;
	auto r_sum = 0;
	auto g_sum = 0;
	auto b_sum = 0;
	auto sample_count = 0;

	for (auto i = 0; i < max_sample_count; ++i)
	{
		const auto& sample_delta = sample_deltas[i];
		const auto new_x = x + sample_delta.dx_;
		const auto new_y = y + sample_delta.dy_;

		if (new_x < 0 || new_x >= width || new_y < 0 || new_y >= height)
		{
			continue;
		}

		const auto pixel_a4r4g4b4 = *(reinterpret_cast<const std::uint16_t*>(
			src_memory + (new_y * src_pitch)) + new_x);

		a_sum += (pixel_a4r4g4b4 >> 12) & 0xF;
		r_sum += (pixel_a4r4g4b4 >> 8) & 0xF;
		g_sum += (pixel_a4r4g4b4 >> 4) & 0xF;
		b_sum += pixel_a4r4g4b4 & 0xF;

		sample_count += 1;
	}

	return static_cast<std::uint16_t>(
		((a_sum / sample_count) << 12) |
		((r_sum / sample_count) << 8) |
		((g_sum / sample_count) << 4) |
		(b_sum / sample_count)
	);
}

std::uint16_t OglRendererImpl::TextureImpl::interpolate_pixel_linearly_a8r8g8b8_to_v8u8(
	const int x,
	const int y,
	const int width,
	const int height,
	const std::uint8_t* src_memory)
{
	constexpr auto max_sample_count = 4;

	struct SampleDelta
	{
		int dx_;
		int dy_;
	}; // SampleDelta

	constexpr SampleDelta sample_deltas[max_sample_count] =
	{
		{0, -1}, {-1, 0}, {0, 1}, {1, 0},
	};

	const auto src_pitch = 4 * width;

	auto g_sum = 0;
	auto b_sum = 0;
	auto sample_count = 0;

	for (auto i = 0; i < max_sample_count; ++i)
	{
		const auto& sample_delta = sample_deltas[i];
		const auto new_x = x + sample_delta.dx_;
		const auto new_y = y + sample_delta.dy_;

		if (new_x < 0 || new_x >= width || new_y < 0 || new_y >= height)
		{
			continue;
		}

		const auto pixel_a8r8g8b8 = *(reinterpret_cast<const std::uint32_t*>(
			src_memory + (new_y * src_pitch)) + new_x);

		g_sum += static_cast<std::int8_t>((pixel_a8r8g8b8 >> 8) & 0xFF);
		b_sum += static_cast<std::int8_t>(pixel_a8r8g8b8 & 0xFF);

		sample_count += 1;
	}

	return static_cast<std::uint16_t>(
		(((g_sum / sample_count) & 0xFF) << 8) |
		((b_sum / sample_count) & 0xFF)
	);
}

//
// OglRendererImpl::TextureImpl
// ==========================================================================


// ==========================================================================
// OglRenderer::Texture::InitializeParam
//

OglRenderer::Texture::InitializeParam::InitializeParam()
	:
	type_{},
	surface_format_{},
	width_{},
	height_{},
	level_count_{}
{
}

bool OglRenderer::Texture::InitializeParam::is_valid() const
{
	switch (type_)
	{
	case Type::two_d:
	case Type::cube_map:
		break;

	default:
		assert(!"Unsupported texture type.");
		return false;
	}

	switch (surface_format_)
	{
	case SurfaceFormat::a4r4g4b4:
	case SurfaceFormat::a8r8g8b8:
	case SurfaceFormat::v8u8:
	case SurfaceFormat::dxt1:
	case SurfaceFormat::dxt3:
	case SurfaceFormat::dxt5:
		break;

	default:
		assert(!"Unsupported surface format.");
		return false;
	}

	if (width_ <= 0 || height_ <= 0)
	{
		assert(!"Unsupported texture size.");
		return false;
	}

	if (level_count_ < 0)
	{
		assert(!"Negative level count.");
		return false;
	}

	return true;
}

//
// OglRenderer::Texture::InitializeParam
// ==========================================================================


// ==========================================================================
// OglRenderer::Texture::UploadParam
//

OglRenderer::Texture::UploadParam::UploadParam()
	:
	has_linear_filter_{},
	level_{},
	cube_face_index_{},
	src_surface_format_{},
	src_pitch_{},
	raw_data_{}
{
}

bool OglRenderer::Texture::UploadParam::is_valid() const
{
	if (level_ < 0)
	{
		assert(!"Negative level.");
		return false;
	}

	if (!is_surface_format_valid(src_surface_format_))
	{
		assert(!"Unsupported source surface format.");
		return false;
	}

	if (src_pitch_ <= 0)
	{
		assert(!"Non-positive source pitch.");
		return false;
	}

	if (!raw_data_)
	{
		assert(!"No source data.");
		return false;
	}

	return true;
}

//
// OglRenderer::Texture::UploadParam
// ==========================================================================


// ==========================================================================
// OglRenderer::Texture
//

OglRenderer::Texture::Texture()
{
}

OglRenderer::Texture::~Texture()
{
}

bool OglRenderer::Texture::initialize(
	const InitializeParam& param)
{
	return do_initialize(param);
}

void OglRenderer::Texture::uninitialize()
{
	do_uninitialize();
}

bool OglRenderer::Texture::upload_level(
	const UploadParam& param)
{
	return do_upload_level(param);
}

OglRenderer::Texture::Type OglRenderer::Texture::get_type() const
{
	return do_get_type();
}

OglRenderer::SurfaceFormat OglRenderer::Texture::get_surface_format() const
{
	return do_get_surface_format();
}

bool OglRenderer::Texture::is_compressed() const
{
	return do_is_compressed();
}

int OglRenderer::Texture::get_width() const
{
	return do_get_width();
}

int OglRenderer::Texture::get_height() const
{
	return do_get_height();
}

int OglRenderer::Texture::get_level_count() const
{
	return do_get_level_count();
}

int OglRenderer::Texture::get_level_width(
	const int level) const
{
	return do_get_level_width(level);
}

int OglRenderer::Texture::get_level_height(
	const int level) const
{
	return do_get_level_height(level);
}

//
// OglRenderer::Texture
// ==========================================================================


// ==========================================================================
// OglRenderer
//

OglRenderer::OglRenderer()
{
}

OglRenderer::~OglRenderer()
{
}

bool OglRenderer::is_initialized() const
{
	return do_is_initialized();
}

bool OglRenderer::initialize(
	const int screen_width,
	const int screen_height)
{
	return do_initialize(screen_width, screen_height);
}

void OglRenderer::uninitialize()
{
	do_set_is_current_context(true);
	do_uninitialize();
}

bool OglRenderer::get_is_current_context() const
{
	return do_get_is_current_context();
}

void OglRenderer::set_is_current_context(
	const bool is_current)
{
	do_set_is_current_context(is_current);
}

bool OglRenderer::set_post_is_current_context(
	const bool is_current_context)
{
	const auto old_is_current_context = do_get_is_current_context();
	do_set_is_current_context(is_current_context);
	return old_is_current_context;
}

void OglRenderer::set_clear_color(
	const std::uint8_t r,
	const std::uint8_t g,
	const std::uint8_t b,
	const std::uint8_t a)
{
	do_set_clear_color(r, g, b, a);
}

const OglRenderer::Viewport& OglRenderer::get_viewport() const
{
	return do_get_viewport();
}

void OglRenderer::clear(
	const ClearFlags clear_flags)
{
	do_clear(clear_flags);
}

void OglRenderer::set_viewport(
	const Viewport& viewport)
{
	do_set_viewport(viewport);
}

OglRenderer::CullMode OglRenderer::get_cull_mode() const
{
	return do_get_cull_mode();
}

void OglRenderer::set_cull_mode(
	const CullMode cull_mode)
{
	do_set_cull_mode(cull_mode);
}

OglRenderer::FillMode OglRenderer::get_fill_mode() const
{
	return do_get_fill_mode();
}

void OglRenderer::set_fill_mode(
	const FillMode fill_mode)
{
	do_set_fill_mode(fill_mode);
}

bool OglRenderer::get_is_clipping() const
{
	return do_get_is_clipping();
}

void OglRenderer::set_is_clipping(
	const bool is_clipping)
{
	do_set_is_clipping(is_clipping);
}

bool OglRenderer::is_depth_enabled() const
{
	return do_is_depth_enabled();
}

void OglRenderer::set_is_depth_enabled(
	const bool is_enabled)
{
	do_set_is_depth_enabled(is_enabled);
}

bool OglRenderer::is_depth_writable() const
{
	return do_is_depth_writable();
}

void OglRenderer::set_is_depth_writable(
	const bool is_writable)
{
	do_set_is_depth_writable(is_writable);
}

OglRenderer::CompareFunc OglRenderer::get_depth_func() const
{
	return do_get_depth_func();
}

void OglRenderer::set_depth_func(
	const CompareFunc depth_func)
{
	do_set_depth_func(depth_func);
}

bool OglRenderer::get_is_blending_enabled() const
{
	return do_get_is_blending_enabled();
}

void OglRenderer::set_is_blending_enabled(
	const bool is_blending_enabled)
{
	do_set_is_blending_enabled(is_blending_enabled);
}

OglRenderer::BlendingFactor OglRenderer::get_src_blending_factor() const
{
	return do_get_src_blending_factor();
}

OglRenderer::BlendingFactor OglRenderer::get_dst_blending_factor() const
{
	return do_get_dst_blending_factor();
}

void OglRenderer::set_blending_factors(
	const BlendingFactor src_blending_function,
	const BlendingFactor dst_blending_function)
{
	do_set_blending_factors(src_blending_function, dst_blending_function);
}

const float* OglRenderer::get_world_matrix(
	const int index) const
{
	return do_get_world_matrix(index);
}

void OglRenderer::set_world_matrix(
	const int index,
	const float* const world_matrix_ptr)
{
	do_set_world_matrix(index, world_matrix_ptr);
}

const float* OglRenderer::get_view_matrix() const
{
	return do_get_view_matrix();
}

void OglRenderer::set_view_matrix(
	const float* const view_matrix_ptr)
{
	do_set_view_matrix(view_matrix_ptr);
}

const float* OglRenderer::get_projection_matrix() const
{
	return do_get_projection_matrix();
}

void OglRenderer::set_projection_matrix(
	const float* const projection_matrix_ptr)
{
	do_set_projection_matrix(projection_matrix_ptr);
}

OglRenderer::SamplerStatePtr OglRenderer::get_sampler_state(
	const int index)
{
	return do_get_sampler_state(index);
}

OglRenderer::VertexArrayObjectPtr OglRenderer::add_vertex_array_object()
{
	return do_add_vertex_array_object();
}

void OglRenderer::remove_vertex_array_object(
	VertexArrayObjectPtr vertex_array_object)
{
	do_remove_vertex_array_object(vertex_array_object);
}

OglRenderer::TexturePtr OglRenderer::add_texture()
{
	return do_add_texture();
}

void OglRenderer::remove_texture(
	TexturePtr texture)
{
	do_remove_texture(texture);
}

void OglRenderer::draw(
	const PrimitiveType primitive_type,
	const std::uint32_t d3d_fvf,
	const int primitive_count,
	const void* const raw_data)
{
	do_draw(primitive_type, d3d_fvf, primitive_count, raw_data);
}

void OglRenderer::ogl_clear_error()
{
	static_cast<void>(ogl_is_succeed());
}

bool OglRenderer::ogl_is_succeed()
{
	const auto max_error_flags = 5;

	auto is_failed = false;

	for (auto i = 0; i < (max_error_flags + 1); ++i)
	{
		const auto ogl_error = ::glGetError();

		if (ogl_error == GL_NO_ERROR)
		{
			break;
		}
		else
		{
			is_failed = true;
		}

		if (i == max_error_flags)
		{
			assert(!"No current context.");
			break;
		}
	}

	return !is_failed;
}

OglRenderer& OglRenderer::get_instance()
{
	static OglRendererImpl instance{};
	return instance;
}

//
// OglRenderer
// ==========================================================================


} // ltjs
