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


constexpr auto invalid_ogl_enum = GLenum{0xFFFFFFFF};
constexpr auto invalid_ogl_format = GLint{-1};


template<typename T>
T ogl_resolve_symbol(
	const char* const symbol_name)
{
	return reinterpret_cast<T>(::wglGetProcAddress(symbol_name));
}


} // namespace


// ==========================================================================
// OglRendererImpl
//

class OglRendererImpl final :
	public OglRenderer
{
public:
	static constexpr auto float_size = 4;
	static constexpr auto dword_size = 4;
	static constexpr auto inv_255_f = 1.0F / 255.0F;


	OglRendererImpl()
		:
		is_initialized_{},
#if 0
		is_current_context_{},
		ogl_dc_{},
		ogl_rc_{},
#else
		is_current_context_{true},
#endif
		extensions_{},
		has_gl_arb_clip_control_{},
		has_gl_ext_clip_volume_hint_{},
		has_gl_arb_texture_compression_{},
		has_gl_ext_texture_compression_s3tc_{},
		has_gl_ext_texture_filter_anisotropic_{},
		max_anisotropy_{},
		screen_width_{},
		screen_height_{},
		is_modified_{},
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
		is_alpha_test_modified_{},
		is_alpha_test_enabled_modified_{},
		is_alpha_test_enabled_{},
		u_is_alpha_test_enabled_{},
		is_alpha_test_func_modified_{},
		alpha_test_func_{},
		u_alpha_test_func_{},
		is_alpha_test_ref_modified_{},
		alpha_test_ref_{},
		u_alpha_test_ref_{},
		texture_factor_{},
		u_texture_factor_{},
		vertex_shader_{},
		fragment_shader_{},
		program_{},
		has_diffuse_{},
		u_has_diffuse_{},
		tcs_count_{},
		u_tcs_count_{},
		is_position_transformed_{},
		are_u_model_views_modified_{},
		is_u_projection_modified_{},
		world_matrices_{},
		view_matrix_{},
		projection_matrix_{},
		u_model_views_{},
		u_projection_{},
		are_texture_matrices_modified_{},
		u_tx_mats_{},
		texture_matrices_{},
		current_vao_{},
		ui_vaos_{},
		vaos_{},
		textures_{},
		samplers_{},
		max_texture_lod_bias_{},
		stages_{},
		texture_units_{},
		current_texture_unit_index_{}
	{
	}

	~OglRendererImpl() override
	{
		assert(!is_initialized_);
	}


	static bool is_surface_format_valid(
		const std::uint32_t surface_format)
	{
		switch (surface_format)
		{
		case d3dfmt_a4r4g4b4:
		case d3dfmt_a8r8g8b8:
		case d3dfmt_v8u8:
		case d3dfmt_dxt1:
		case d3dfmt_dxt3:
		case d3dfmt_dxt5:
			return true;

		default:
			return false;
		}
	}

	static GLenum usage_flags_to_ogl_usage(
		const std::uint32_t usage_flags)
	{
		if ((usage_flags & OglRenderer::d3dusage_dynamic) != 0)
		{
			if ((usage_flags & OglRenderer::d3dusage_writeonly) == 0)
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
			if ((usage_flags & OglRenderer::d3dusage_writeonly) == 0)
			{
				return GL_STATIC_COPY;
			}
			else
			{
				return GL_STATIC_DRAW;
			}
		}
	}

	static bool is_fvf_valid(
		const std::uint32_t fvf)
	{
		constexpr auto all_valid_flags =
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

		if ((fvf & ~all_valid_flags) != 0)
		{
			assert(!"Unsupported flag.");
			return false;
		}

		const auto has_position = (
			((fvf & d3dfvf_xyz) == d3dfvf_xyz && (fvf & d3dfvf_xyzrhw) != d3dfvf_xyzrhw) ||
			(fvf & d3dfvf_xyzb1) == d3dfvf_xyzb1 ||
			(fvf & d3dfvf_xyzb2) == d3dfvf_xyzb2 ||
			(fvf & d3dfvf_xyzb3) == d3dfvf_xyzb3
		);

		const auto has_transformed_position = ((fvf & d3dfvf_xyzrhw) == d3dfvf_xyzrhw);

		if (!has_position && !has_transformed_position)
		{
			assert(!"No position.");
			return false;
		}

		if (has_position && has_transformed_position)
		{
			assert(!"Position and transformed position are mutually exclusive.");
			return false;
		}

		const auto has_normal = ((fvf & d3dfvf_normal) == d3dfvf_normal);

		if (has_normal && has_transformed_position)
		{
			assert(!"Normal for transformed position not supported.");
			return false;
		}

		return true;
	}

	static int get_fvf_tex_coord_count(
		const std::uint32_t fvf)
	{
		if ((fvf & d3dfvf_tex4) != 0)
		{
			return 4;
		}
		else if ((fvf & d3dfvf_tex3) != 0)
		{
			return 3;
		}
		else if ((fvf & d3dfvf_tex2) != 0)
		{
			return 2;
		}
		else
		{
			return 0;
		}
	}

	static int get_fvf_tex_coord_item_count(
		const std::uint32_t fvf,
		const int set_index)
	{
		if ((fvf & d3dfvf_texcoordsize4(set_index)) == d3dfvf_texcoordsize4(set_index))
		{
			return 4;
		}
		else if ((fvf & d3dfvf_texcoordsize3(set_index)) == d3dfvf_texcoordsize3(set_index))
		{
			return 3;
		}
		else
		{
			return 2;
		}
	}

	static int get_fvf_position_item_count(
		const std::uint32_t fvf)
	{
		const auto has_position = (
			((fvf & d3dfvf_xyz) == d3dfvf_xyz && (fvf & d3dfvf_xyzrhw) != d3dfvf_xyzrhw) ||
			(fvf & d3dfvf_xyzb1) == d3dfvf_xyzb1 ||
			(fvf & d3dfvf_xyzb2) == d3dfvf_xyzb2 ||
			(fvf & d3dfvf_xyzb3) == d3dfvf_xyzb3
		);

		const auto has_transformed_position = ((fvf & d3dfvf_xyzrhw) == d3dfvf_xyzrhw);

		if (has_position)
		{
			return 3;
		}
		else if (has_transformed_position)
		{
			return 4;
		}
		else
		{
			assert(!"No position.");
			return 0;
		}
	}

	static int calculate_fvf_position_size(
		const std::uint32_t fvf)
	{
		return get_fvf_position_item_count(fvf) * float_size;
	}

	static int get_fvf_blending_weight_count(
		const std::uint32_t fvf)
	{
		if ((fvf & d3dfvf_xyzb3) == d3dfvf_xyzb3)
		{
			return 3;
		}
		else if ((fvf & d3dfvf_xyzb2) == d3dfvf_xyzb2)
		{
			return 2;
		}
		else if ((fvf & d3dfvf_xyzb1) == d3dfvf_xyzb1)
		{
			return 1;
		}
		else
		{
			return 0;
		}
	}

	static int calculate_fvf_blending_weights_size(
		const std::uint32_t fvf)
	{
		return get_fvf_blending_weight_count(fvf) * float_size;
	}

	static int get_fvf_normal_item_count(
		const std::uint32_t fvf)
	{
		if ((fvf & d3dfvf_normal) == d3dfvf_normal)
		{
			return 3;
		}
		else
		{
			return 0;
		}
	}

	static int calculate_fvf_normal_size(
		const std::uint32_t fvf)
	{
		return get_fvf_normal_item_count(fvf) * float_size;
	}

	static int calculate_fvf_diffuse_size(
		const std::uint32_t fvf)
	{
		if ((fvf & d3dfvf_diffuse) == d3dfvf_diffuse)
		{
			return 1 * dword_size;
		}
		else
		{
			return 0;
		}
	}

	static int calculate_fvf_tex_coord_set_size(
		const std::uint32_t fvf,
		const int set_index)
	{
		const auto item_count = get_fvf_tex_coord_item_count(fvf, set_index);
		return item_count * float_size;
	}

	static int calculate_fvf_tex_coord_sets_size(
		const std::uint32_t fvf)
	{
		const auto set_count = get_fvf_tex_coord_count(fvf);

		auto sets_size = 0;

		for (auto i = 0; i < set_count; ++i)
		{
			sets_size += calculate_fvf_tex_coord_set_size(fvf, i);
		}

		return sets_size;
	}

	static int calculate_fvf_vertex_size(
		const std::uint32_t fvf)
	{
		const auto position_size = calculate_fvf_position_size(fvf);
		const auto bweights_size = calculate_fvf_blending_weights_size(fvf);
		const auto normal_size = calculate_fvf_normal_size(fvf);
		const auto diffuse_size = calculate_fvf_diffuse_size(fvf);
		const auto tex_sets_size = calculate_fvf_tex_coord_sets_size(fvf);

		const auto vertex_size =
			position_size +
			bweights_size +
			normal_size +
			diffuse_size +
			tex_sets_size +
			0;

		return vertex_size;
	}


private:
	// Use the extra one to upload texture data.
	static constexpr auto max_texture_units = max_stages + 1;

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


	using Vector4F = std::array<float, 4>;

	using Matrix4F = std::array<float, 16>;
	using Matrix2F = std::array<float, 4>;

	using WorldMatrix = Matrix4F;
	using WorldMatrices = std::array<WorldMatrix, max_world_matrices>;

	using ModelViewMatrixModificationFlags = std::bitset<max_world_matrices>;
	using UModelViewMatrices = std::array<GLint, max_world_matrices>;

	using ViewMatrix = Matrix4F;
	using ModelViewMatrix = Matrix4F;
	using ProjectionMatrix = Matrix4F;

	using TextureMatrix = Matrix4F;
	using TextureMatrices = std::array<TextureMatrix, max_stages>;
	using UTextureMatrices = std::array<GLint, max_stages>;
	using TextureMatrixModificationFlags = std::bitset<max_stages>;


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

	using UiVaoPtr = UiVao * ;

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

		std::uint32_t fvf_;
		std::uint32_t vertex_usage_flags_;
		std::uint32_t index_usage_flags_;
		int vertex_size_;
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
			const std::uint32_t primitive_type,
			const int index_base,
			const int vertex_base,
			const int primitive_count) override;

		void uninitialize_internal();
	}; // VertexArrayObjectImpl

	using VertexArrayObjectUPtr = std::unique_ptr<VertexArrayObjectImpl>;
	using VertexArrayObjectUList = std::list<VertexArrayObjectUPtr>;

	class TextureImpl;
	using TextureImplPtr = TextureImpl*;

	class TextureImpl :
		public Texture
	{
	public:
		TextureImpl(
			OglRendererImpl& ogl_renderer);

		~TextureImpl() override;

		void bind(
			const bool is_binded);


	private:
		static constexpr auto max_cube_faces = 6;

		static constexpr auto max_interpolation_sample_count = 4;

		struct InterpolationSampleDelta
		{
			int dx_;
			int dy_;
		}; // InterpolationSampleDelta

		using InterpolationSampleDeltas = std::array<InterpolationSampleDelta, max_interpolation_sample_count>;

		static constexpr InterpolationSampleDeltas interpolation_sample_deltas =
		{{
			{0, -1}, {-1, 0}, {0, 1}, {1, 0},
		}}; // interpolation_sample_deltas


		using Buffer = std::vector<ul::UnValue<std::uint8_t>>;
		using OglCubeMapFaceMap = std::array<GLenum, max_cube_faces>;


		static const OglCubeMapFaceMap ogl_cube_map_face_map;


		OglRendererImpl& ogl_renderer_;

		bool is_initialized_;

		GLuint ogl_texture_;
		GLenum ogl_target_;

		Type type_;
		bool is_compressed_;
		std::uint32_t surface_format_;

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

		std::uint32_t do_get_surface_format() const override;

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

		void bind_internal(
			const bool is_binded);

		TextureImpl*& get_binding();
	}; // TextureImpl


	class SamplerImpl :
		public Sampler
	{
	public:
		SamplerImpl(
			OglRendererImpl& ogl_renderer,
			const int index);

		~SamplerImpl() override;


		bool is_initialized() const;

		void apply_modifications();


	private:
		static constexpr auto default_addressing_mode_u = d3dtaddress_wrap;
		static constexpr auto default_addressing_mode_v = d3dtaddress_wrap;

		static constexpr auto default_mag_filter = d3dtexf_point;
		static constexpr auto default_min_filter = d3dtexf_point;
		static constexpr auto default_mip_filter = d3dtexf_none;

		static constexpr auto default_lod_bias = 0.0F;

		static constexpr auto default_max_anisotropy = min_anisotropy;


		OglRendererImpl& ogl_renderer_;

		bool is_initialized_;
		bool is_modified_;

		const int index_;
		GLuint ogl_sampler_;

		bool is_addressing_mode_u_modified_;
		std::uint32_t addressing_mode_u_;

		bool is_addressing_mode_v_modified_;
		std::uint32_t addressing_mode_v_;

		bool is_mag_filter_modified_;
		std::uint32_t mag_filter_;

		bool is_minmip_filter_modified_;
		std::uint32_t min_filter_;
		std::uint32_t mip_filter_;

		bool is_lod_bias_modified_;
		float lod_bias_;
		float max_lod_bias_;

		bool has_anisotropy_;
		bool is_anisotropy_modified_;
		float anisotropy_;
		float max_anisotropy_;


		// ========================================================================
		// API
		//

		std::uint32_t do_get_addressing_mode_u() const override;

		void do_set_addressing_mode_u(
			const std::uint32_t addressing_mode_u) override;


		std::uint32_t do_get_addressing_mode_v() const override;

		void do_set_addressing_mode_v(
			const std::uint32_t addressing_mode_v) override;


		std::uint32_t do_get_mag_filter() const override;

		void do_set_mag_filter(
			const std::uint32_t mag_filter) override;


		std::uint32_t do_get_min_filter() const override;

		void do_set_min_filter(
			const std::uint32_t min_filter) override;


		std::uint32_t do_get_mip_filter() const override;

		void do_set_mip_filter(
			const std::uint32_t mip_filter) override;


		float do_get_lod_bias() const override;

		void do_set_lod_bias(
			const float lod_bias) override;


		float do_get_anisotropy() const override;

		void do_set_anisotropy(
			const float anisotropy) override;

		//
		// API
		// ========================================================================


		void initialize_internal();

		void uninitialize_internal();


		void set_addressing_mode_u_internal();

		void set_addressing_mode_v_internal();

		void set_mag_filter_internal();

		void set_minmip_filter_internal();

		void set_lod_bias_internal();

		void set_anisotropy_internal();

		void set_defaults();


		void apply_addressing_mode_u_modifications();

		void apply_addressing_mode_v_modifications();

		void apply_mag_filter_modifications();

		void apply_minmip_filter_modifications();

		void apply_lod_bias_modifications();

		void apply_anisotropy_modifications();


		static GLenum get_ogl_addressing_mode(
			const std::uint32_t d3d_addressing_mode);

		static GLenum get_ogl_mag_filter(
			const std::uint32_t d3d_mag_filter);

		static GLenum get_ogl_min_filter(
			const std::uint32_t d3d_min_filter,
			const std::uint32_t d3d_mip_filter);
	}; // SamplerImpl

	using Samplers = std::vector<SamplerImpl>;


	class StageImpl :
		public Stage
	{
	public:
		StageImpl(
			OglRendererImpl& ogl_renderer,
			const int index);

		~StageImpl() override;


		bool initialize();

		void uninitialize();

		bool is_initialized() const;


		void apply_modifications();


	private:
		static TextureImplPtr default_texture;

		static std::uint32_t default_color_op_0;
		static std::uint32_t default_color_op_n;
		static std::uint32_t default_color_arg1;
		static std::uint32_t default_color_arg2;

		static std::uint32_t default_alpha_op_0;
		static std::uint32_t default_alpha_op_n;
		static std::uint32_t default_alpha_arg1;
		static std::uint32_t default_alpha_arg2;

		static std::uint32_t default_trans_flags;

		static float default_bump_map_lum_scale;
		static float default_bump_map_lum_offset;
		static Matrix2F default_bump_map_matrix;


		OglRendererImpl& ogl_renderer_;
		const int index_;

		bool is_initialized_;
		bool is_modified_;

		bool is_texture_modified_;
		TextureImplPtr texture_;
		int u_is_2d_;
		int u_sampler_2d_;
		int u_sampler_cube_;

		bool is_color_op_modified_;
		std::uint32_t color_op_;
		int u_color_op_;

		bool is_color_arg1_modified_;
		std::uint32_t color_arg1_;
		int u_color_arg1_;

		bool is_color_arg2_modified_;
		std::uint32_t color_arg2_;
		int u_color_arg2_;

		bool is_alpha_op_modified_;
		std::uint32_t alpha_op_;
		int u_alpha_op_;

		bool is_alpha_arg1_modified_;
		std::uint32_t alpha_arg1_;
		int u_alpha_arg1_;

		bool is_alpha_arg2_modified_;
		std::uint32_t alpha_arg2_;
		int u_alpha_arg2_;

		bool is_coord_index_modified_;
		std::uint32_t coord_index_;
		int u_coord_index_;

		bool is_trans_flags_modified_;
		std::uint32_t trans_flags_;
		int u_trans_flags_;

		bool is_bump_map_lum_scale_modified_;
		float bump_map_lum_scale_;

		bool is_bump_map_lum_offset_modified_;
		float bump_map_lum_offset_;

		bool is_bump_map_matrix_modified_;
		Matrix2F bump_map_matrix_;


		// ==================================================================
		// API
		//

		TexturePtr do_get_texture() override;

		void do_set_texture(
			TexturePtr texture) override;


		std::uint32_t do_get_color_op() const override;

		void do_set_color_op(
			const std::uint32_t color_op) override;


		std::uint32_t do_get_color_arg1() const override;

		void do_set_color_arg1(
			const std::uint32_t color_arg1) override;


		std::uint32_t do_get_color_arg2() const override;

		void do_set_color_arg2(
			const std::uint32_t color_arg2) override;


		std::uint32_t do_get_alpha_op() const override;

		void do_set_alpha_op(
			const std::uint32_t alpha_op) override;


		std::uint32_t do_get_alpha_arg1() const override;

		void do_set_alpha_arg1(
			const std::uint32_t alpha_arg1) override;


		std::uint32_t do_get_alpha_arg2() const override;

		void do_set_alpha_arg2(
			const std::uint32_t alpha_arg2) override;


		std::uint32_t do_get_coord_index() const override;

		void do_set_coord_index(
			const std::uint32_t coord_index) override;


		std::uint32_t do_get_trans_flags() const override;

		void do_set_trans_flags(
			const std::uint32_t trans_flags) override;


		float do_get_bump_map_lum_scale() const override;

		void do_set_bump_map_lum_scale(
			const float lum_scale) override;


		float do_get_bump_map_lum_offset() const override;

		void do_set_bump_map_lum_offset(
			const float lum_offset) override;


		float do_get_bump_map_coefficient(
			const int index) const override;

		void do_set_bump_map_coefficient(
			const int index,
			const float coefficient) override;

		//
		// API
		// ==================================================================


		bool initialize_internal();

		void uninitialize_internal();

		void set_defaults();

		void locate_uniforms();


		void set_texture_internal();

		void set_color_op_internal();

		void set_color_arg1_internal();

		void set_color_arg2_internal();

		void set_alpha_op_internal();

		void set_alpha_arg1_internal();

		void set_alpha_arg2_internal();

		void set_coord_index_internal();

		void set_trans_flags_internal();

		void set_bump_map_lum_scale_internal();

		void set_bump_map_lum_offset_internal();

		void set_bump_map_matrix_internal();


		void apply_texture_modifications();

		void apply_color_op_modifications();

		void apply_color_arg1_modifications();

		void apply_color_arg2_modifications();

		void apply_alpha_op_modifications();

		void apply_alpha_arg1_modifications();

		void apply_alpha_arg2_modifications();

		void apply_coord_index_modifications();

		void apply_trans_flags_modifications();

		void apply_bump_map_lum_scale_modifications();

		void apply_bump_map_lum_offset_modifications();

		void apply_bump_map_matrix_modifications();


		static bool is_op_valid(
			const std::uint32_t op);

		static bool is_arg_valid(
			const std::uint32_t arg);

		bool are_trans_flags_valid(
			const std::uint32_t trans_flags);

		bool is_coord_index_valid(
			const std::uint32_t coord_index);
	}; // StageImpl

	using SamplerImplPtr = SamplerImpl*;


	struct TextureUnit
	{
		using Targets = std::array<TextureImplPtr, Texture::max_types>;


		int index_;

		SamplerImplPtr sampler_;
		Targets targets_;
	}; // TextureUnit

	using TextureUnits = std::array<TextureUnit, max_texture_units>;


	using TextureImplUPtr = std::unique_ptr<TextureImpl>;
	using TextureImplUList = std::list<TextureImplUPtr>;

	using Stages = std::vector<StageImpl>;
	using StagesBindings = std::array<TextureImplPtr, max_stages>;


	using ViewportSize = std::array<int, 2>;
	using Extensions = std::vector<std::string>;

	enum class ShaderType
	{
		none,
		vertex,
		fragment,
	}; // ShaderType


	bool is_initialized_;
#if 0
	bool is_current_context_;

	HDC ogl_dc_;
	HGLRC ogl_rc_;
#else
	bool is_current_context_ = true;
#endif

	Extensions extensions_;

	bool has_gl_arb_clip_control_;
	bool has_gl_ext_clip_volume_hint_;
	bool has_gl_arb_texture_compression_;
	bool has_gl_ext_texture_compression_s3tc_;

	bool has_gl_ext_texture_filter_anisotropic_;
	float max_anisotropy_;

	int screen_width_;
	int screen_height_;

	bool is_modified_;

	std::uint8_t clear_color_r_;
	std::uint8_t clear_color_g_;
	std::uint8_t clear_color_b_;
	std::uint8_t clear_color_a_;

	Viewport viewport_;
	ViewportSize max_viewport_size_;

	std::uint32_t cull_mode_;
	std::uint32_t fill_mode_;

	bool is_clipping_;

	bool is_depth_enabled_;
	bool is_depth_writable_;
	std::uint32_t depth_func_;

	bool is_blending_enabled_;
	std::uint32_t src_blending_factor_;
	std::uint32_t dst_blending_factor_;

	bool is_alpha_test_modified_;

	bool is_alpha_test_enabled_modified_;
	bool is_alpha_test_enabled_;
	GLint u_is_alpha_test_enabled_;

	bool is_alpha_test_func_modified_;
	std::uint32_t alpha_test_func_;
	GLint u_alpha_test_func_;

	bool is_alpha_test_ref_modified_;
	std::uint32_t alpha_test_ref_;
	GLint u_alpha_test_ref_;

	std::uint32_t texture_factor_;
	GLint u_texture_factor_;

	GLuint vertex_shader_;
	GLuint fragment_shader_;
	GLuint program_;

	bool has_diffuse_;
	GLint u_has_diffuse_;

	int tcs_count_;
	GLint u_tcs_count_;

	bool is_position_transformed_;

	ModelViewMatrixModificationFlags are_u_model_views_modified_;
	bool is_u_projection_modified_;

	WorldMatrices world_matrices_;
	ViewMatrix view_matrix_;
	ProjectionMatrix projection_matrix_;

	UModelViewMatrices u_model_views_;
	GLint u_projection_;

	TextureMatrixModificationFlags are_texture_matrices_modified_;
	UTextureMatrices u_tx_mats_;
	TextureMatrices texture_matrices_;

	VertexArrayObjectImpl* current_vao_;

	UiVaos ui_vaos_;
	VertexArrayObjectUList vaos_;

	TextureImplUList textures_;

	Samplers samplers_;
	float max_texture_lod_bias_;

	Stages stages_;

	TextureUnits texture_units_;
	int current_texture_unit_index_;


	static int default_viewport_x;
	static int default_viewport_y;
	static float default_viewport_depth_min_z;
	static float default_viewport_depth_max_z;

	static std::uint8_t default_clear_color_r;
	static std::uint8_t default_clear_color_g;
	static std::uint8_t default_clear_color_b;
	static std::uint8_t default_clear_color_a;

	static const std::uint32_t default_cull_mode;
	static const std::uint32_t default_fill_mode;

	static const bool default_is_clipping;

	static const bool default_is_depth_enabled;
	static const bool default_is_depth_writable;
	static const std::uint32_t default_depth_func;

	static const bool default_is_blending_enabled;
	static const std::uint32_t default_src_blending_factor;
	static const std::uint32_t default_dst_blending_factor;

	static const bool default_is_alpha_test_enabled;
	static const std::uint32_t default_alpha_test_func;
	static const std::uint32_t default_alpha_test_ref;

	static const std::uint32_t default_texture_factor;

	static const bool default_has_diffuse;

	static const int default_tcs_count;

	static const Matrix4F identity_matrix;

	static const std::string shader_d3d_consts_string;
	static const std::string shader_stage_struct_string;

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

#if 0
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
			//assert(!"Invalid state.");
			return;
		}

		if (is_current == is_current_context_)
		{
			return;
		}

		set_current_context_internal(is_current);
	}
#endif

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
		const std::uint32_t clear_flags) override
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
			viewport.x_ >= static_cast<std::uint32_t>(max_viewport_size_[0]) ||
			viewport.y_ >= static_cast<std::uint32_t>(max_viewport_size_[1]) ||
			viewport.width_ >= static_cast<std::uint32_t>(max_viewport_size_[0]) ||
			viewport.height_ >= static_cast<std::uint32_t>(max_viewport_size_[1]))
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
			is_modified_ = true;
			is_u_projection_modified_ = true;

			set_viewport_size_internal();
		}

		if (is_depth_changed)
		{
			set_viewport_depth_internal();
		}
	}

	std::uint32_t do_get_cull_mode() const override
	{
		assert(is_initialized_);
		return cull_mode_;
	}

	void do_set_cull_mode(
		const std::uint32_t cull_mode) override
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

		const auto is_old_enabled = (cull_mode_ != d3dcull_none);
		const auto is_new_enabled = (cull_mode != d3dcull_none);

		cull_mode_ = cull_mode;

		set_cull_mode_internal(is_old_enabled != is_new_enabled);
	}

	std::uint32_t do_get_fill_mode() const override
	{
		assert(is_initialized_);
		return fill_mode_;
	}

	void do_set_fill_mode(
		const std::uint32_t fill_mode) override
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

		is_modified_ = true;
		is_u_projection_modified_ = true;
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

	std::uint32_t do_get_depth_func() const override
	{
		assert(is_initialized_);
		return depth_func_;
	}

	void do_set_depth_func(
		const std::uint32_t depth_func) override
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

	std::uint32_t do_get_src_blending_factor() const override
	{
		assert(is_initialized_);
		return src_blending_factor_;
	}

	std::uint32_t do_get_dst_blending_factor() const override
	{
		assert(is_initialized_);
		return dst_blending_factor_;
	}

	void do_set_blending_factors(
		const std::uint32_t src_factor,
		const std::uint32_t dst_factor) override
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

	bool do_get_is_alpha_test_enabled() const override
	{
		assert(!is_initialized_);
		return is_alpha_test_enabled_;
	}

	void do_set_is_alpha_test_enabled(
		const bool is_alpha_test_enabled) override
	{
		if (!is_initialized_ || !is_current_context_)
		{
			assert(!"Invalid state.");
			return;
		}

		if (is_alpha_test_enabled == is_alpha_test_enabled_)
		{
			return;
		}

		is_alpha_test_enabled_ = is_alpha_test_enabled;

		is_modified_ = true;
		is_alpha_test_modified_ = true;
		is_alpha_test_enabled_modified_ = true;
	}

	std::uint32_t do_get_alpha_test_func() const override
	{
		assert(!is_initialized_);
		return alpha_test_func_;
	}

	void do_set_alpha_test_func(
		const std::uint32_t d3d9_alpha_test_func) override
	{
		if (!is_initialized_ || !is_current_context_)
		{
			assert(!"Invalid state.");
			return;
		}

		if (d3d9_alpha_test_func == alpha_test_func_)
		{
			return;
		}

		alpha_test_func_ = d3d9_alpha_test_func;

		is_modified_ = true;
		is_alpha_test_modified_ = true;
		is_alpha_test_func_modified_ = true;
	}

	std::uint32_t do_get_alpha_test_ref() const override
	{
		assert(!is_initialized_);
		return alpha_test_ref_;
	}

	void do_set_alpha_test_ref(
		const std::uint32_t d3d9_alpha_test_ref) override
	{
		if (!is_initialized_ || !is_current_context_)
		{
			assert(!"Invalid state.");
			return;
		}

		if (d3d9_alpha_test_ref == alpha_test_ref_)
		{
			return;
		}

		alpha_test_ref_ = d3d9_alpha_test_ref;

		is_modified_ = true;
		is_alpha_test_modified_ = true;
		is_alpha_test_ref_modified_ = true;
	}

	std::uint32_t do_get_texture_factor() const override
	{
		assert(is_initialized_);
		return texture_factor_;
	}

	void do_set_texture_factor(
		const std::uint32_t d3d_texture_factor) override
	{
		if (!is_initialized_ || !is_current_context_)
		{
			assert(!"Invalid state.");
			return;
		}

		if (d3d_texture_factor == texture_factor_)
		{
			return;
		}

		texture_factor_ = d3d_texture_factor;

		set_texture_factor_internal();
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

		is_modified_ = true;
		are_u_model_views_modified_.set(index);
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

		is_modified_ = true;
		are_u_model_views_modified_.set();
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

		is_modified_ = true;
		is_u_projection_modified_ = true;
	}

	const float* do_get_texture_matrix(
		const int index) const override
	{
		assert(is_initialized_);
		assert(index >= 0 && index < max_stages);
		return texture_matrices_[index].data();
	}

	void do_set_texture_matrix(
		const int index,
		const float* const texture_matrix_ptr) override
	{
		if (!is_initialized_)
		{
			assert(!"Invalid state.");
			return;
		}

		if (index < 0 || index >= max_stages)
		{
			assert(!"Index out of range.");
			return;
		}

		if (!texture_matrix_ptr)
		{
			assert(!"No matrix.");
			return;
		}

		const auto new_matrix = *reinterpret_cast<const TextureMatrix*>(texture_matrix_ptr);

		if (new_matrix == texture_matrices_[index])
		{
			return;
		}

		texture_matrices_[index] = new_matrix;

		is_modified_ = true;
		are_texture_matrices_modified_.set(index);
	}

	SamplerPtr do_get_sampler(
		const int index) override
	{
		return &samplers_[index];
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

#if 0
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
#endif

		textures_.remove_if(
			[&](const auto& item_uptr)
		{
			return item_uptr.get() == texture;
		}
		);

		const auto size_after = textures_.size();

		assert(size_before > size_after);
	}

	Stage& do_get_stage(
		const int stage_index) override
	{
		if (!is_initialized_)
		{
			assert(!"Invalid state.");
			throw "Invalid state.";
		}

		if (stage_index < 0 || stage_index >= max_stages)
		{
			assert(!"Stage index out of range.");
			throw "Stage index out of range.";
		}

		return stages_[stage_index];
	}

	void do_draw(
		const std::uint32_t primitive_type,
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
			assert(!"Unsupported flexible vertex format.");
			return;
		}

		auto& ui_vao = *ui_vao_it;

		auto vao = ui_vao.vao_;

		if (!vao)
		{
			assert(!"No vertex array object.");
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

#if 0
		// Get current context (must exist).
		//
		ogl_dc_ = ::wglGetCurrentDC();
		ogl_rc_ = ::wglGetCurrentContext();

		if (!ogl_dc_ || !ogl_rc_)
		{
			return false;
		}
#endif

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

		if (!initialize_stages())
		{
			return false;
		}

		initialize_texture_units();

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
		set_default_alpha_test();
		set_default_texture_factor();
		set_default_world_matrices();
		set_default_view_matrix();
		set_default_projection_matrix();
		set_default_texture_matrices();
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
#if 0
		is_current_context_ = false;

		ogl_dc_ = nullptr;
		ogl_rc_ = nullptr;
#endif

		extensions_.clear();

		has_gl_arb_clip_control_ = false;
		has_gl_ext_clip_volume_hint_ = false;
		has_gl_arb_texture_compression_ = false;
		has_gl_ext_texture_compression_s3tc_ = false;

		has_gl_ext_texture_filter_anisotropic_ = false;
		max_anisotropy_ = 0.0F;

		screen_width_ = 0;
		screen_height_ = 0;

		is_modified_ = false;

		clear_color_r_ = 0;
		clear_color_g_ = 0;
		clear_color_b_ = 0;
		clear_color_a_ = 0;

		viewport_ = {};
		max_viewport_size_ = {};

		cull_mode_ = d3dcull_none;
		fill_mode_ = 0;

		is_clipping_ = false;

		is_depth_enabled_ = false;
		is_depth_writable_ = false;
		depth_func_ = 0;

		is_blending_enabled_ = false;
		src_blending_factor_ = 0;
		dst_blending_factor_ = 0;

		is_alpha_test_modified_ = false;

		is_alpha_test_enabled_modified_ = false;
		is_alpha_test_enabled_ = false;
		u_is_alpha_test_enabled_ = -1;

		is_alpha_test_func_modified_ = false;
		alpha_test_func_ = 0;
		u_alpha_test_func_ = -1;

		is_alpha_test_ref_modified_ = false;
		alpha_test_ref_ = 0;
		u_alpha_test_ref_ = -1;

		texture_factor_ = 0;
		u_texture_factor_ = -1;

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

		tcs_count_ = 0;
		u_tcs_count_ = -1;

		is_position_transformed_ = false;

		are_u_model_views_modified_ = {};
		is_u_projection_modified_ = false;

		world_matrices_ = {};
		view_matrix_ = {};
		projection_matrix_ = {};

		u_model_views_.fill(-1);
		u_projection_ = -1;

		are_texture_matrices_modified_ = {};
		u_tx_mats_.fill(-1);
		texture_matrices_ = {};

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

		uninitialize_samplers();
		uninitialize_stages();
		uninitialize_texture_units();
		uninitialize_textures();
	}

#if 0
	void set_current_context_internal(
		const bool is_current)
	{
		const auto dc = (is_current ? ogl_dc_ : nullptr);
		const auto rc = (is_current ? ogl_rc_ : nullptr);

		const auto make_result = ::wglMakeCurrent(dc, rc);
		assert(make_result);

		is_current_context_ = is_current;
	}
#endif

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
		const auto is_cull_face_enabled = (cull_mode_ != d3dcull_none);

		switch (cull_mode_)
		{
		case d3dcull_none:
			break;

		case d3dcull_cw:
			::glCullFace(GL_FRONT);
			assert(ogl_is_succeed());
			break;

		case d3dcull_ccw:
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
			clear_color_r_ * inv_255_f,
			clear_color_g_ * inv_255_f,
			clear_color_b_ * inv_255_f,
			clear_color_a_ * inv_255_f);

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

	void set_alpha_test_is_enabled_internal()
	{
		::glUniform1i(u_is_alpha_test_enabled_, is_alpha_test_enabled_);
		assert(ogl_is_succeed());
	}

	void set_default_alpha_test_is_enabled()
	{
		is_modified_ = true;
		is_alpha_test_modified_ = true;
		is_alpha_test_enabled_modified_ = true;

		is_alpha_test_enabled_ = default_is_alpha_test_enabled;
	}

	void set_alpha_test_func_internal()
	{
		::glUniform1ui(u_alpha_test_func_, alpha_test_func_);
		assert(ogl_is_succeed());
	}

	void set_default_alpha_test_func()
	{
		is_modified_ = true;
		is_alpha_test_modified_ = true;
		is_alpha_test_func_modified_ = true;

		alpha_test_func_ = default_alpha_test_func;
	}

	void set_alpha_test_ref_internal()
	{
		::glUniform1ui(u_alpha_test_ref_, alpha_test_ref_);
		assert(ogl_is_succeed());
	}

	void set_default_alpha_test_ref()
	{
		is_modified_ = true;
		is_alpha_test_modified_ = true;
		is_alpha_test_ref_modified_ = true;

		alpha_test_ref_ = default_alpha_test_ref;
	}

	void set_default_alpha_test()
	{
		set_default_alpha_test_is_enabled();
		set_default_alpha_test_func();
		set_default_alpha_test_ref();
	}

	void apply_is_alpha_test_enabled_modifications()
	{
		if (!is_alpha_test_enabled_modified_)
		{
			return;
		}

		set_alpha_test_is_enabled_internal();

		is_alpha_test_enabled_modified_ = false;
	}

	void apply_alpha_test_func_modifications()
	{
		if (!is_alpha_test_func_modified_)
		{
			return;
		}

		set_alpha_test_func_internal();

		is_alpha_test_func_modified_ = false;
	}

	void apply_alpha_test_ref_modifications()
	{
		if (!is_alpha_test_ref_modified_)
		{
			return;
		}

		set_alpha_test_ref_internal();

		is_alpha_test_ref_modified_ = false;
	}

	void apply_alpha_test_modifications()
	{
		if (!is_alpha_test_modified_)
		{
			return;
		}

		apply_is_alpha_test_enabled_modifications();
		apply_alpha_test_func_modifications();
		apply_alpha_test_ref_modifications();

		is_alpha_test_modified_ = false;
	}

	void set_texture_factor_internal()
	{
		const auto ogl_color = Vector4F
		{
			((texture_factor_ >> 16) & 0xFF) * inv_255_f,
			((texture_factor_ >> 8) & 0xFF) * inv_255_f,
			((texture_factor_ >> 0) & 0xFF) * inv_255_f,
			((texture_factor_ >> 24) & 0xFF) * inv_255_f,
		};

		::glUniform4fv(u_texture_factor_, 1, ogl_color.data());
		assert(ogl_is_succeed());
	}

	void set_default_texture_factor()
	{
		texture_factor_ = default_texture_factor;
		set_texture_factor_internal();
	}

	void set_uniform_defaults()
	{
		has_diffuse_ = default_has_diffuse;
		set_has_diffuse_internal();

		tcs_count_ = default_tcs_count;
		set_tcs_count_internal();
	}

	void add_ui_vaos()
	{
		auto param = VertexArrayObject::InitializeParam{};
		param.vertex_count_ = max_ui_vao_vertices;
		param.vertex_usage_flags_ = d3dusage_dynamic;

		for (auto i = 0; i < max_ui_vaos; ++i)
		{
			auto vao = add_vertex_array_object();

			if (vao)
			{
				ui_vaos_[i].vao_ = vao;

				param.fvf_ = ui_vao_d3d_fvfs[i];

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

	int locate_uniform(
		const std::string& uniform_name) const
	{
		return ::glGetUniformLocation(program_, uniform_name.c_str());
	}

	bool locate_uniforms()
	{
		u_texture_factor_ = ::glGetUniformLocation(program_, "u_texture_factor");
		assert(ogl_is_succeed());
		assert(u_texture_factor_ >= 0);

		u_has_diffuse_ = ::glGetUniformLocation(program_, "u_has_diffuse");
		assert(ogl_is_succeed());
		assert(u_has_diffuse_ >= 0);

		u_tcs_count_ = ::glGetUniformLocation(program_, "u_tcs_count");
		assert(ogl_is_succeed());
		assert(u_tcs_count_ >= 0);

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

		for (auto i = 0; i < max_stages; ++i)
		{
			const auto uniform_name = "u_tx_mats[" + std::to_string(i) + "]";
			u_tx_mats_[i] = ::glGetUniformLocation(program_, uniform_name.c_str());
			assert(ogl_is_succeed());
			assert(u_tx_mats_[i] >= 0);
		}

		u_is_alpha_test_enabled_ = ::glGetUniformLocation(program_, "u_is_alpha_test_enabled");
		assert(ogl_is_succeed());
		assert(u_is_alpha_test_enabled_ >= 0);

		u_alpha_test_func_ = ::glGetUniformLocation(program_, "u_alpha_test_func");
		assert(ogl_is_succeed());
		assert(u_alpha_test_func_ >= 0);

		u_alpha_test_ref_ = ::glGetUniformLocation(program_, "u_alpha_test_ref");
		assert(ogl_is_succeed());
		assert(u_alpha_test_ref_ >= 0);

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

	void set_tcs_count_internal()
	{
		::glUniform1i(u_tcs_count_, tcs_count_);
		assert(ogl_is_succeed());
	}

	void set_tcs_count(
		const int tcs_count)
	{
		if (!is_initialized_)
		{
			return;
		}

		if (tcs_count == tcs_count_)
		{
			return;
		}

		tcs_count_ = tcs_count;

		set_tcs_count_internal();
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

		is_modified_ = true;
		are_u_model_views_modified_.set();
		is_u_projection_modified_ = true;
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

		is_modified_ = true;
		are_u_model_views_modified_.set();
	}

	void set_default_view_matrix()
	{
		view_matrix_ = identity_matrix;

		is_modified_ = true;
		are_u_model_views_modified_.set();
	}

	void set_default_projection_matrix()
	{
		projection_matrix_ = identity_matrix;

		is_modified_ = true;
		is_u_projection_modified_ = true;
	}

	void set_default_texture_matrices()
	{
		texture_matrices_.fill(identity_matrix);

		is_modified_ = true;
		are_texture_matrices_modified_.set();
	}

	void apply_u_view_model_modifications(
		const int index)
	{
		if (!are_u_model_views_modified_.test(index))
		{
			return;
		}

		are_u_model_views_modified_.reset(index);

		set_u_model_view(index);
	}

	void apply_u_view_models_modifications()
	{
		for (auto i = 0; i < max_world_matrices; ++i)
		{
			apply_u_view_model_modifications(i);
		}
	}

	void apply_u_projection_modifications()
	{
		if (!is_u_projection_modified_)
		{
			return;
		}

		is_u_projection_modified_ = false;

		set_u_projection();
	}

	void apply_texture_matrix_modifications(
		const int index)
	{
		::glUniformMatrix4fv(u_tx_mats_[index], 1, GL_FALSE, texture_matrices_[index].data());
		assert(ogl_is_succeed());
	}

	void apply_texture_matrices_modifications()
	{
		if (!are_texture_matrices_modified_.any())
		{
			return;
		}

		for (auto i = 0; i < max_stages; ++i)
		{
			if (are_texture_matrices_modified_.test(i))
			{
				apply_texture_matrix_modifications(i);
			}
		}

		are_texture_matrices_modified_.reset();
	}

	void apply_samplers_modifications()
	{
		for (auto& sampler : samplers_)
		{
			sampler.apply_modifications();
		}
	}

	void apply_stages_modifications()
	{
		for (auto& stage : stages_)
		{
			stage.apply_modifications();
		}
	}

	void apply_modifications()
	{
		if (!is_modified_)
		{
			return;
		}

		is_modified_ = false;

		apply_u_view_models_modifications();
		apply_u_projection_modifications();
		apply_texture_matrices_modifications();
		apply_samplers_modifications();
		apply_stages_modifications();
		apply_alpha_test_modifications();
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
		samplers_.clear();
		samplers_.reserve(max_stages);

		auto index = 0;

		for (auto i_sampler = 0; i_sampler < max_stages; ++i_sampler)
		{
			samplers_.emplace_back(*this, i_sampler);

			auto& sampler = samplers_.back();

			if (!sampler.is_initialized())
			{
				return false;
			}

			index += 1;
		}

		return true;
	}

	void uninitialize_textures()
	{
		textures_.clear();
	}

	void uninitialize_samplers()
	{
		samplers_.clear();
		max_texture_lod_bias_ = 0.0F;
	}

	bool initialize_stages()
	{
		stages_.clear();
		stages_.reserve(max_stages);

		auto index = 0;

		for (auto i_stage = 0; i_stage < max_stages; ++i_stage)
		{
			stages_.emplace_back(*this, i_stage);

			auto& stage = stages_.back();

			if (!stage.initialize())
			{
				return false;
			}

			index += 1;
		}

		return true;
	}

	void uninitialize_stages()
	{
		stages_.clear();
	}

	void initialize_texture_units()
	{
		uninitialize_texture_units();

		::glActiveTexture(GL_TEXTURE0 + current_texture_unit_index_);
		assert(ogl_is_succeed());
	}

	void uninitialize_texture_units()
	{
		texture_units_.fill({});
		current_texture_unit_index_ = 0;
	}

	void set_texture_unit(
		const int texture_unit_index)
	{
		if (texture_unit_index == current_texture_unit_index_)
		{
			return;
		}

		current_texture_unit_index_ = texture_unit_index;

		::glActiveTexture(GL_TEXTURE0 + current_texture_unit_index_);
		assert(ogl_is_succeed());
	}

	void set_texture_for_unit(
		const int texture_unit_index,
		TextureImplPtr texture)
	{
		set_texture_unit(texture_unit_index);

		if (texture)
		{
			texture->bind(true);
		}
		else
		{
			auto& targets = texture_units_[current_texture_unit_index_].targets_;

			for (auto& target : targets)
			{
				if (target)
				{
					target->bind(false);
				}
			}
		}
	}

	static GLbitfield get_ogl_clear_flags(
		const std::uint32_t d3d_clear_flags)
	{
		auto ogl_clear_flags = GLbitfield{};

		if ((d3d_clear_flags & d3dclear_target) != 0)
		{
			ogl_clear_flags |= GL_COLOR_BUFFER_BIT;
		}

		if ((d3d_clear_flags & d3dclear_zbuffer) != 0)
		{
			ogl_clear_flags |= GL_DEPTH_BUFFER_BIT;
		}

		if ((d3d_clear_flags & d3dclear_stencil) != 0)
		{
			ogl_clear_flags |= GL_STENCIL_BUFFER_BIT;
		}

		return ogl_clear_flags;
	}

	static GLenum get_ogl_fill_mode(
		const std::uint32_t d3d_fill_mode)
	{
		switch (d3d_fill_mode)
		{
		case d3dfill_wireframe:
			return GL_LINE;

		case d3dfill_solid:
			return GL_FILL;

		default:
			assert(!"Unsupported fill mode.");
			return invalid_ogl_enum;
		}
	}

	static GLenum get_ogl_compare_func(
		const std::uint32_t d3d_depth_func)
	{
		switch (d3d_depth_func)
		{
		case d3dcmp_always:
			return GL_ALWAYS;

		case d3dcmp_equal:
			return GL_EQUAL;

		case d3dcmp_greater:
			return GL_GREATER;

		case d3dcmp_greaterequal:
			return GL_GEQUAL;

		case d3dcmp_less:
			return GL_LESS;

		case d3dcmp_lessequal:
			return GL_LEQUAL;

		case d3dcmp_notequal:
			return GL_NOTEQUAL;

		default:
			assert(!"Invalid compare function.");
			return invalid_ogl_enum;
		}
	}

	static GLenum get_ogl_blending_factor(
		const std::uint32_t blending_factor)
	{
		switch (blending_factor)
		{
		case d3dblend_zero:
			return GL_ZERO;

		case d3dblend_one:
			return GL_ONE;

		case d3dblend_srcalpha:
			return GL_SRC_ALPHA;

		case d3dblend_srccolor:
			return GL_SRC_COLOR;

		case d3dblend_invsrcalpha:
			return GL_ONE_MINUS_SRC_ALPHA;

		case d3dblend_invsrccolor:
			return GL_ONE_MINUS_SRC_COLOR;

		case d3dblend_destcolor:
			return GL_DST_COLOR;

		case d3dblend_invdestcolor:
			return GL_ONE_MINUS_DST_COLOR;

		default:
			assert(!"Unsupported blending factor.");
			return invalid_ogl_enum;
		}
	}

	static int calculate_vertex_count(
		const std::uint32_t primitive_type,
		const int primitive_count)
	{
		if (primitive_count <= 0)
		{
			assert(!"Invalid state.");
			return 0;
		}

		switch (primitive_type)
		{
		case d3dpt_linelist:
			return 2 * primitive_count;

		case d3dpt_trianglefan:
		case d3dpt_trianglestrip:
			return 3 + (primitive_count - 1);

		case d3dpt_trianglelist:
			return 3 * primitive_count;

		default:
			assert(!"Unsupported primitive type.");
			return 0;
		}
	}

	static GLenum get_ogl_primitive_type(
		const std::uint32_t primitive_type)
	{
		switch (primitive_type)
		{
		case d3dpt_linelist:
			return GL_LINES;

		case d3dpt_trianglefan:
			return GL_TRIANGLE_FAN;

		case d3dpt_trianglestrip:
			return GL_TRIANGLE_STRIP;

		case d3dpt_trianglelist:
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

const std::uint32_t OglRendererImpl::default_cull_mode = d3dcull_ccw;
const std::uint32_t OglRendererImpl::default_fill_mode = d3dfill_solid;

const bool OglRendererImpl::default_is_clipping = true;

const bool OglRendererImpl::default_is_depth_enabled = true;
const bool OglRendererImpl::default_is_depth_writable = true;
const std::uint32_t OglRendererImpl::default_depth_func = d3dcmp_lessequal;

const bool OglRendererImpl::default_is_blending_enabled = false;
const std::uint32_t OglRendererImpl::default_src_blending_factor = d3dblend_one;
const std::uint32_t OglRendererImpl::default_dst_blending_factor = d3dblend_zero;

const bool OglRendererImpl::default_is_alpha_test_enabled = false;
const std::uint32_t OglRendererImpl::default_alpha_test_func = d3dcmp_greater;
const std::uint32_t OglRendererImpl::default_alpha_test_ref = 0x00000000;

const std::uint32_t OglRendererImpl::default_texture_factor = 0xFFFFFFFF;

const bool OglRendererImpl::default_has_diffuse = false;

const int OglRendererImpl::default_tcs_count = 0;

const OglRendererImpl::Matrix4F OglRendererImpl::identity_matrix =
{
	1.0F, 0.0F, 0.0F, 0.0F,
	0.0F, 1.0F, 0.0F, 0.0F,
	0.0F, 0.0F, 1.0F, 0.0F,
	0.0F, 0.0F, 0.0F, 1.0F,
};


const std::string OglRendererImpl::vertex_shader_source = std::string{
R"SHADER(

#version 330 core


// Maximum model-view matrix count.
const int max_model_view_count = 4;

// Maximum stages.
const int max_stages = 4;

// Default diffuse color.
const vec4 default_diffuse = vec4(1, 1, 1, 1);


//
// Direct3D 9 compare functions.
//

const uint d3dcmp_less = 2U;
const uint d3dcmp_equal = 3U;
const uint d3dcmp_lessequal = 4U;
const uint d3dcmp_greater = 5U;
const uint d3dcmp_notequal = 6U;
const uint d3dcmp_greaterequal = 7U;
const uint d3dcmp_always = 8U;


//
// Direct3D 9 texture coordinate index flags.
//

const uint d3dtss_tci_passthru = 0x00000000U;
const uint d3dtss_tci_cameraspaceposition = 0x00020000U;
const uint d3dtss_tci_cameraspacereflectionvector = 0x00030000U;


//
// Direct3D 9 texture operation values.
//

const uint d3dtop_disable = 1U;
const uint d3dtop_selectarg1 = 2U;
const uint d3dtop_selectarg2 = 3U;
const uint d3dtop_modulate = 4U;
const uint d3dtop_modulate2x = 5U;
const uint d3dtop_add = 7U;
const uint d3dtop_addsigned = 8U;
const uint d3dtop_subtract = 10U;
const uint d3dtop_blendcurrentalpha = 16U;
const uint d3dtop_modulatealpha_addcolor = 18U;
const uint d3dtop_bumpenvmap = 22U;
const uint d3dtop_bumpenvmapluminance = 23U;
const uint d3dtop_dotproduct3 = 24U;


//
// Direct3D 9 texture argument values.
//

const uint d3dta_diffuse = 0x00000000U;
const uint d3dta_current = 0x00000001U;
const uint d3dta_texture = 0x00000002U;
const uint d3dta_tfactor = 0x00000003U;
const uint d3dta_complement = 0x00000010U;


//
// Direct3D 9 texture transformation values.
//

const uint d3dttff_disable = 0U;
const uint d3dttff_count2 = 2U;
const uint d3dttff_count3 = 3U;
const uint d3dttff_projected = 256U;


// Texture stage.
struct Stage
{
	// Is texture 2D (true) or cube map (false)?
	bool is_2d;

	// Texture coordinates index.
	uint coord_index;

	// Transformation flags.
	uint trans_flags;

	// Color operation.
	uint color_op;

	// First argument for color operation.
	uint color_arg1;

	// Second argument for color operation.
	uint color_arg2;

	// Alpha operation.
	uint alpha_op;

	// First argument for alpha operation.
	uint alpha_arg1;

	// Second argument for alpha operation.
	uint alpha_arg2;
}; // Stage


layout(location = 0) in vec4 a_position;
layout(location = 1) in vec4 a_bweights;
layout(location = 2) in vec4 a_normal;
layout(location = 3) in vec4 a_diffuse;
layout(location = 4) in vec4 a_tcss[max_stages];


out vec4 v_diffuse;
out vec4 v_tcss[max_stages];
flat out int v_stage_count;


// Has diffuse attribute?
uniform bool u_has_diffuse = false;

// Current count of texture coordinate set.
uniform int u_tcs_count = 0;

// Model-view matrices.
uniform mat4 u_model_views[max_model_view_count];

// Projection matrix.
uniform mat4 u_projection;

// Texture transformation matrices.
uniform mat4 u_tx_mats[max_stages];

// Texture stages.
uniform Stage u_stages[max_stages];


vec4 model_view_position;


bool apply_texture_stage(
	int stage_index)
{
	Stage stage = u_stages[stage_index];

	if (stage.color_op == d3dtop_disable)
	{
		return false;
	}

	v_stage_count += 1;

	uint tci_index = stage.coord_index & 0x0000FFFFU;
	uint tci_flags = stage.coord_index & 0xFFFF0000U;

	switch (tci_flags)
	{
	case d3dtss_tci_passthru:
		if (tci_index < uint(u_tcs_count))
		{
			v_tcss[stage_index] = a_tcss[tci_index];
		}
		else
		{
			v_tcss[stage_index] = vec4(0);
		}

		break;

	case d3dtss_tci_cameraspaceposition:
		v_tcss[stage_index] = model_view_position;
		break;

	case d3dtss_tci_cameraspacereflectionvector:
		v_tcss[stage_index] = vec4(reflect(model_view_position.xyz, a_normal.xyz), 1);
		break;
	}

	uint tff_value = stage.trans_flags & 0xFFU;
	uint tff_flags = stage.trans_flags & 0xFFFFFF00U;

	if (tff_value != d3dttff_disable)
	{
		v_tcss[stage_index] = u_tx_mats[stage_index] * v_tcss[stage_index];

		if (tff_value == d3dttff_count2)
		{
			v_tcss[stage_index].zw = vec2(0, 1);
		}
		else if (tff_value == d3dttff_count3)
		{
			if ((tff_flags & d3dttff_projected) != 0U)
			{
				v_tcss[stage_index] = vec4(
					v_tcss[stage_index].x / v_tcss[stage_index].z,
					v_tcss[stage_index].y / v_tcss[stage_index].z,
					0,
					1
				);
			}
			else
			{
				v_tcss[stage_index].w = 1;
			}
		}
		else
		{
			v_tcss[stage_index] = vec4(0);
		}
	}

	return true;
}

void apply_texture_stages()
{
	v_stage_count = 0;

	for (int i = 0; i < max_stages; ++i)
	{
		if (!apply_texture_stage(i))
		{
			break;
		}
	}
}


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

	model_view_position = u_model_views[0] * a_position;

	apply_texture_stages();

	gl_Position = u_projection * model_view_position;
}

)SHADER"
}; // vertex_shader_source

const std::string OglRendererImpl::fragment_shader_source = std::string{
R"SHADER(

#version 330 core


// Maximum model-view matrix count.
const int max_model_view_count = 4;

// Maximum stages.
const int max_stages = 4;

// Default diffuse color.
const vec4 default_diffuse = vec4(1, 1, 1, 1);


//
// Direct3D 9 compare functions.
//

const uint d3dcmp_less = 2U;
const uint d3dcmp_equal = 3U;
const uint d3dcmp_lessequal = 4U;
const uint d3dcmp_greater = 5U;
const uint d3dcmp_notequal = 6U;
const uint d3dcmp_greaterequal = 7U;
const uint d3dcmp_always = 8U;


//
// Direct3D 9 texture coordinate index flags.
//

const uint d3dtss_tci_passthru = 0x00000000U;
const uint d3dtss_tci_cameraspaceposition = 0x00020000U;
const uint d3dtss_tci_cameraspacereflectionvector = 0x00030000U;


//
// Direct3D 9 texture operation values.
//

const uint d3dtop_disable = 1U;
const uint d3dtop_selectarg1 = 2U;
const uint d3dtop_selectarg2 = 3U;
const uint d3dtop_modulate = 4U;
const uint d3dtop_modulate2x = 5U;
const uint d3dtop_add = 7U;
const uint d3dtop_addsigned = 8U;
const uint d3dtop_subtract = 10U;
const uint d3dtop_blendcurrentalpha = 16U;
const uint d3dtop_modulatealpha_addcolor = 18U;
const uint d3dtop_bumpenvmap = 22U;
const uint d3dtop_bumpenvmapluminance = 23U;
const uint d3dtop_dotproduct3 = 24U;


//
// Direct3D 9 texture argument values.
//

const uint d3dta_diffuse = 0x00000000U;
const uint d3dta_current = 0x00000001U;
const uint d3dta_texture = 0x00000002U;
const uint d3dta_tfactor = 0x00000003U;
const uint d3dta_complement = 0x00000010U;


//
// Direct3D 9 texture transformation values.
//

const uint d3dttff_disable = 0U;
const uint d3dttff_count2 = 2U;
const uint d3dttff_count3 = 3U;
const uint d3dttff_projected = 256U;


// Texture stage.
struct Stage
{
	// Is texture 2D (true) or cube map (false)?
	bool is_2d;

	// Texture coordinates index.
	uint coord_index;

	// Transformation flags.
	uint trans_flags;

	// Color operation.
	uint color_op;

	// First argument for color operation.
	uint color_arg1;

	// Second argument for color operation.
	uint color_arg2;

	// Alpha operation.
	uint alpha_op;

	// First argument for alpha operation.
	uint alpha_arg1;

	// Second argument for alpha operation.
	uint alpha_arg2;
}; // Stage


in vec4 v_diffuse;
in vec4 v_tcss[max_stages];
flat in int v_stage_count;


out vec4 o_fragment;


// Texture factor.
uniform vec4 u_texture_factor;

// Texture stages.
uniform Stage u_stages[max_stages];

// 2D texture samplers.
uniform sampler2D u_samplers_2d[max_stages];

// Cube map texture samplers.
uniform samplerCube u_samplers_cube[max_stages];

// Is alpha test enabled?
uniform bool u_is_alpha_test_enabled = false;

// Alpha test function.
uniform uint u_alpha_test_func;

// Alpha test reference value.
uniform uint u_alpha_test_ref;


// Texture blending result.
vec4 tx_current;


bool is_tx_succeed = true;

vec4 get_texel_2d(
	int stage_index)
{
	vec2 tc = v_tcss[stage_index].st;

	switch (stage_index)
	{
	case 0:
		return texture(u_samplers_2d[0], tc);

	case 1:
		return texture(u_samplers_2d[1], tc);

	case 2:
		return texture(u_samplers_2d[2], tc);

	case 3:
		return texture(u_samplers_2d[3], tc);

	default:
		is_tx_succeed = false;
		return vec4(0);
	}
}

vec4 get_texel_cube(
	int stage_index)
{
	vec3 tc = v_tcss[stage_index].stp;

	switch (stage_index)
	{
	case 0:
		return texture(u_samplers_cube[0], tc);

	case 1:
		return texture(u_samplers_cube[1], tc);

	case 2:
		return texture(u_samplers_cube[2], tc);

	case 3:
		return texture(u_samplers_cube[3], tc);

	default:
		is_tx_succeed = false;
		return vec4(0);
	}
}

vec4 get_texel(
	int stage_index)
{
	if (u_stages[stage_index].is_2d)
	{
		return get_texel_2d(stage_index);
	}
	else
	{
		return get_texel_cube(stage_index);
	}
}

vec4 get_texture_color_arg(
	int stage_index,
	uint color_arg)
{
	switch (color_arg & 0xFU)
	{
	case d3dta_current:
		return tx_current;

	case d3dta_diffuse:
		return v_diffuse;

	case d3dta_texture:
		return get_texel(stage_index);

	case d3dta_tfactor:
		return u_texture_factor;

	default:
		is_tx_succeed = false;
		return vec4(0);
	}
}

float get_texture_alpha_arg(
	int stage_index,
	uint alpha_arg)
{
	uint alpha_arg_value = alpha_arg & 0xFU;
	bool is_complement = ((alpha_arg & d3dta_complement) != 0U);

	float result;

	switch (alpha_arg)
	{
	case d3dta_current:
		result = tx_current.a;
		break;

	case d3dta_diffuse:
		result = v_diffuse.a;
		break;

	case d3dta_texture:
		return get_texel(stage_index).a;

	case d3dta_tfactor:
		result = u_texture_factor.a;
		break;

	default:
		result = 0;
		is_tx_succeed = false;
		break;
	}

	if (is_complement)
	{
		result = 1 - result;
	}

	return result;
}


vec4 apply_texture_color_stage(
	int stage_index)
{
	Stage stage = u_stages[stage_index];

	vec4 color_arg1 = get_texture_color_arg(stage_index, stage.color_arg1);
	vec4 color_arg2 = get_texture_color_arg(stage_index, stage.color_arg2);

	switch (stage.color_op)
	{
	case d3dtop_selectarg1:
		return color_arg1;

	case d3dtop_selectarg2:
		return color_arg2;

	case d3dtop_modulate:
		return color_arg1 * color_arg2;

	case d3dtop_modulate2x:
		return 2 * color_arg1 * color_arg2;

	case d3dtop_add:
		return color_arg1 + color_arg2;

	case d3dtop_addsigned:
		return color_arg1 + color_arg2 - 0.5;

	case d3dtop_blendcurrentalpha:
		return mix(color_arg2, color_arg1, tx_current.a);

	case d3dtop_modulatealpha_addcolor:
		return color_arg1 + (color_arg1.a * color_arg2);

	default:
		is_tx_succeed = false;
		return tx_current;
	}
}

float apply_texture_alpha_stage(
	int stage_index)
{
	Stage stage = u_stages[stage_index];

	float alpha_arg1 = get_texture_alpha_arg(stage_index, stage.alpha_arg1);
	float alpha_arg2 = get_texture_alpha_arg(stage_index, stage.alpha_arg2);

	switch (stage.alpha_op)
	{
	case d3dtop_selectarg1:
		return alpha_arg1;

	case d3dtop_selectarg2:
		return alpha_arg2;

	case d3dtop_modulate:
		return alpha_arg1 * alpha_arg2;

	case d3dtop_modulate2x:
		return 2 * alpha_arg1 * alpha_arg2;

	case d3dtop_add:
		return alpha_arg1 + alpha_arg2;

	case d3dtop_subtract:
		return alpha_arg1 - alpha_arg2;

	default:
		is_tx_succeed = false;
		return tx_current.a;
	}
}

void apply_texture_stage(
	int stage_index)
{
	vec4 color = apply_texture_color_stage(stage_index);
	float alpha = apply_texture_alpha_stage(stage_index);
	tx_current = vec4(color.xyz, clamp(alpha, 0, 1));

	if (!is_tx_succeed)
	{
		tx_current = vec4(1, 0, 0, 1);
	}
}

void apply_texture_stages()
{
	tx_current = v_diffuse;

	for (int i = 0; i < v_stage_count; ++i)
	{
		apply_texture_stage(i);
	}
}

void apply_alpha_test()
{
	if (!u_is_alpha_test_enabled)
	{
		return;
	}

	uint alpha = uint(clamp(tx_current.a * 255.0, 0.0, 255.0));
	uint alpha_ref = clamp(u_alpha_test_ref, 0U, 255U);

	switch (u_alpha_test_func)
	{
	case d3dcmp_equal:
		if (alpha != alpha_ref)
		{
			discard;
		}
		break;

	case d3dcmp_greater:
		if (alpha <= alpha_ref)
		{
			discard;
		}
		break;

	case d3dcmp_greaterequal:
		if (alpha < alpha_ref)
		{
			discard;
		}
		break;

	case d3dcmp_less:
		if (alpha >= alpha_ref)
		{
			discard;
		}
		break;

	case d3dcmp_lessequal:
		if (alpha > alpha_ref)
		{
			discard;
		}
		break;

	case d3dcmp_notequal:
		if (alpha == alpha_ref)
		{
			discard;
		}
		break;

	default:
		break;
	}
}

void main()
{
	apply_texture_stages();

	apply_alpha_test();

	o_fragment = tx_current;
}

)SHADER"
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
	fvf_{},
	vertex_usage_flags_{},
	index_usage_flags_{},
	vertex_size_{},
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
	const auto fvf = param.fvf_;
	const auto ogl_vertex_usage = usage_flags_to_ogl_usage(param.vertex_usage_flags_);
	const auto vertex_size = calculate_fvf_vertex_size(fvf);
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
	fvf_ = param.fvf_;

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
		const auto position_item_count = get_fvf_position_item_count(fvf);
		const auto position_size = position_item_count * float_size;

		::glEnableVertexAttribArray(0);
		assert(ogl_is_succeed());
		::glVertexAttribPointer(0, position_item_count, GL_FLOAT, GL_FALSE, vertex_size, component_offset_ptr);
		assert(ogl_is_succeed());

		component_offset_ptr += position_size;
	}


	// Blending weights.
	//
	const auto blending_weights_item_count = get_fvf_blending_weight_count(param.fvf_);

	if (blending_weights_item_count > 0)
	{
		const auto blending_weights_size = blending_weights_item_count * float_size;

		::glEnableVertexAttribArray(1);
		assert(ogl_is_succeed());
		::glVertexAttribPointer(1, blending_weights_item_count, GL_FLOAT, GL_FALSE, vertex_size, component_offset_ptr);
		assert(ogl_is_succeed());

		component_offset_ptr += blending_weights_size;
	}

	// Normal.
	//
	const auto normal_item_count = get_fvf_normal_item_count(fvf);

	if (normal_item_count > 0)
	{
		const auto normals_size = normal_item_count * float_size;

		::glEnableVertexAttribArray(2);
		assert(ogl_is_succeed());
		::glVertexAttribPointer(2, normal_item_count, GL_FLOAT, GL_FALSE, vertex_size, component_offset_ptr);
		assert(ogl_is_succeed());

		component_offset_ptr += normals_size;
	}

	// Diffuse.
	//
	const auto diffuse_size = calculate_fvf_diffuse_size(fvf);

	if (diffuse_size > 0)
	{
		::glEnableVertexAttribArray(3);
		assert(ogl_is_succeed());
		::glVertexAttribPointer(3, GL_BGRA, GL_UNSIGNED_BYTE, GL_TRUE, vertex_size, component_offset_ptr);
		assert(ogl_is_succeed());

		component_offset_ptr += diffuse_size;
	}

	// Texture coordinate sets.
	//
	const auto tex_coord_set_count = get_fvf_tex_coord_count(fvf);

	if (tex_coord_set_count > 0)
	{
		auto base_array_index = 4;

		for (auto i = 0; i < tex_coord_set_count; ++i)
		{
			const auto array_index = base_array_index + i;
			const auto tex_coord_set_item_count = get_fvf_tex_coord_item_count(fvf, i);
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
	vertex_size_ = vertex_size;
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

	const auto data_offset = vertex_index * vertex_size_;
	const auto data_size = vertex_count * vertex_size_;

	::glBindBuffer(GL_ARRAY_BUFFER, ogl_vertex_buffer_);
	assert(ogl_is_succeed());
	::glBufferSubData(GL_ARRAY_BUFFER, data_offset, data_size, raw_data);
	assert(ogl_is_succeed());
	::glBindBuffer(GL_ARRAY_BUFFER, 0);
	assert(ogl_is_succeed());
}

void OglRendererImpl::VertexArrayObjectImpl::do_draw(
	const std::uint32_t primitive_type,
	const int index_base,
	const int vertex_base,
	const int primitive_count)
{
	if (!is_initialized_ || index_base < 0 || vertex_base < 0 || primitive_count <= 0)
	{
		return;
	}

	const auto vertex_count = calculate_vertex_count(primitive_type, primitive_count);
	const auto ogl_primitive_type = get_ogl_primitive_type(primitive_type);

	if (vertex_count <= 0 || ogl_primitive_type == invalid_ogl_enum)
	{
		return;
	}

	const auto has_diffuse = ((fvf_ & d3dfvf_diffuse) == d3dfvf_diffuse);
	ogl_renderer_.set_has_diffuse(has_diffuse);

	const auto tcs_count = get_fvf_tex_coord_count(fvf_);
	ogl_renderer_.set_tcs_count(tcs_count);

	const auto is_position_transformed = ((fvf_ & d3dfvf_xyzrhw) == d3dfvf_xyzrhw);
	ogl_renderer_.set_is_position_transformed(is_position_transformed);

	ogl_renderer_.apply_modifications();

	if (ogl_renderer_.current_vao_ != this)
	{
		ogl_renderer_.current_vao_ = this;

		::glBindVertexArray(ogl_vao_);
		assert(ogl_is_succeed());
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
	fvf_ = 0;
	vertex_usage_flags_ = 0;
	index_usage_flags_ = 0;
	vertex_size_ = 0;
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
	fvf_{},
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
	if (!OglRendererImpl::is_fvf_valid(fvf_))
	{
		return false;
	}

	const auto ogl_vertex_usage = OglRendererImpl::usage_flags_to_ogl_usage(vertex_usage_flags_);

	if (!ogl_vertex_usage || vertex_count_ <= 0)
	{
		return false;
	}

	if (has_index_)
	{
		const auto ogl_index_usage = OglRendererImpl::usage_flags_to_ogl_usage(index_usage_flags_);

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
	const std::uint32_t primitive_type,
	const int primitive_count)
{
	do_draw(primitive_type, 0, 0, primitive_count);
}

void OglRendererImpl::VertexArrayObject::draw(
	const std::uint32_t primitive_type,
	const int vertex_base,
	const int primitive_count)
{
	do_draw(primitive_type, 0, vertex_base, primitive_count);
}

void OglRendererImpl::VertexArrayObject::draw(
	const std::uint32_t primitive_type,
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
// OglRendererImpl::SamplerImpl
//

OglRendererImpl::SamplerImpl::SamplerImpl(
	OglRendererImpl& ogl_renderer,
	const int index)
	:
	ogl_renderer_{ogl_renderer},
	is_initialized_{},
	is_modified_{},
	index_{index},
	ogl_sampler_{},
	is_addressing_mode_u_modified_{},
	addressing_mode_u_{},
	is_addressing_mode_v_modified_{},
	addressing_mode_v_{},
	is_mag_filter_modified_{},
	mag_filter_{},
	is_minmip_filter_modified_{},
	min_filter_{},
	mip_filter_{},
	is_lod_bias_modified_{},
	lod_bias_{},
	max_lod_bias_{},
	has_anisotropy_{},
	is_anisotropy_modified_{},
	anisotropy_{},
	max_anisotropy_{}
{
	initialize_internal();
}

OglRendererImpl::SamplerImpl::~SamplerImpl()
{
	uninitialize_internal();
}

void OglRendererImpl::SamplerImpl::initialize_internal()
{
	uninitialize_internal();

	::glGenSamplers(1, &ogl_sampler_);
	assert(ogl_is_succeed());

	::glBindSampler(index_, ogl_sampler_);
	assert(ogl_is_succeed());

	if (!ogl_is_succeed())
	{
		uninitialize_internal();
		return;
	}

	set_defaults();

	is_initialized_ = true;
}

void OglRendererImpl::SamplerImpl::uninitialize_internal()
{
	is_initialized_ = false;
	is_modified_ = false;

	if (ogl_sampler_)
	{
		::glDeleteSamplers(1, &ogl_sampler_);
		assert(ogl_is_succeed());

		ogl_sampler_ = 0;
	}

	is_addressing_mode_u_modified_ = false;
	addressing_mode_u_ = 0;

	is_addressing_mode_v_modified_ = false;
	addressing_mode_v_ = 0;

	is_mag_filter_modified_ = false;
	mag_filter_ = d3dtexf_none;

	is_minmip_filter_modified_ = false;
	min_filter_ = d3dtexf_none;
	mip_filter_ = d3dtexf_none;

	is_lod_bias_modified_ = false;
	lod_bias_ = 0.0F;
	max_lod_bias_ = 0.0F;

	has_anisotropy_ = false;
	is_anisotropy_modified_ = false;
	anisotropy_ = 0;
	max_anisotropy_ = 0;
}

bool OglRendererImpl::SamplerImpl::is_initialized() const
{
	return is_initialized_;
}

void OglRendererImpl::SamplerImpl::apply_modifications()
{
	if (!is_modified_)
	{
		return;
	}

	is_modified_ = false;

	apply_addressing_mode_u_modifications();
	apply_addressing_mode_v_modifications();
	apply_mag_filter_modifications();
	apply_minmip_filter_modifications();
	apply_lod_bias_modifications();
	apply_anisotropy_modifications();
}

std::uint32_t OglRendererImpl::SamplerImpl::do_get_addressing_mode_u() const
{
	assert(is_initialized_);
	return addressing_mode_u_;
}

void OglRendererImpl::SamplerImpl::do_set_addressing_mode_u(
	const std::uint32_t addressing_mode_u)
{
	if (!is_initialized_)
	{
		assert(!"Invalid state.");
		return;
	}

	switch (addressing_mode_u)
	{
	case d3dtaddress_clamp:
	case d3dtaddress_wrap:
		break;

	default:
		assert(!"Unsupported texture U-addressing mode.");
		return;
	}

	if (addressing_mode_u_ == addressing_mode_u)
	{
		return;
	}

	is_modified_ = true;
	ogl_renderer_.is_modified_ = true;
	is_addressing_mode_u_modified_ = true;
	addressing_mode_u_ = addressing_mode_u;
}

std::uint32_t OglRendererImpl::SamplerImpl::do_get_addressing_mode_v() const
{
	assert(is_initialized_);
	return addressing_mode_v_;
}

void OglRendererImpl::SamplerImpl::do_set_addressing_mode_v(
	const std::uint32_t addressing_mode_v)
{
	if (!is_initialized_)
	{
		assert(!"Invalid state.");
		return;
	}

	switch (addressing_mode_v)
	{
	case d3dtaddress_clamp:
	case d3dtaddress_wrap:
		break;

	default:
		assert(!"Unsupported texture V-addressing mode.");
		return;
	}

	if (addressing_mode_v_ == addressing_mode_v)
	{
		return;
	}

	is_modified_ = true;
	ogl_renderer_.is_modified_ = true;
	is_addressing_mode_v_modified_ = true;
	addressing_mode_v_ = addressing_mode_v;
}

std::uint32_t OglRendererImpl::SamplerImpl::do_get_mag_filter() const
{
	assert(is_initialized_);
	return mag_filter_;
}

void OglRendererImpl::SamplerImpl::do_set_mag_filter(
	const std::uint32_t mag_filter)
{
	if (!is_initialized_)
	{
		assert(!"Invalid state.");
		return;
	}

	if (mag_filter_ == mag_filter)
	{
		return;
	}

	is_modified_ = true;
	ogl_renderer_.is_modified_ = true;
	is_mag_filter_modified_ = true;
	mag_filter_ = mag_filter;
}

std::uint32_t OglRendererImpl::SamplerImpl::do_get_min_filter() const
{
	assert(is_initialized_);
	return min_filter_;
}

void OglRendererImpl::SamplerImpl::do_set_min_filter(
	const std::uint32_t min_filter)
{
	if (!is_initialized_)
	{
		assert(!"Invalid state.");
		return;
	}

	if (min_filter_ == min_filter)
	{
		return;
	}

	is_modified_ = true;
	ogl_renderer_.is_modified_ = true;
	is_minmip_filter_modified_ = true;
	min_filter_ = min_filter;
}

std::uint32_t OglRendererImpl::SamplerImpl::do_get_mip_filter() const
{
	assert(is_initialized_);
	return mip_filter_;
}

void OglRendererImpl::SamplerImpl::do_set_mip_filter(
	const std::uint32_t mip_filter)
{
	if (!is_initialized_)
	{
		assert(!"Invalid state.");
		return;
	}

	if (mip_filter_ == mip_filter)
	{
		return;
	}

	is_modified_ = true;
	ogl_renderer_.is_modified_ = true;
	is_minmip_filter_modified_ = true;
	mip_filter_ = mip_filter;
}

float OglRendererImpl::SamplerImpl::do_get_lod_bias() const
{
	assert(is_initialized_);
	return lod_bias_;
}

void OglRendererImpl::SamplerImpl::do_set_lod_bias(
	const float lod_bias)
{
	if (!is_initialized_)
	{
		assert(!"Invalid state.");
		return;
	}

	if (lod_bias_ == lod_bias)
	{
		return;
	}

	is_modified_ = true;
	ogl_renderer_.is_modified_ = true;
	is_lod_bias_modified_ = true;
	lod_bias_ = lod_bias;
}

float OglRendererImpl::SamplerImpl::do_get_anisotropy() const
{
	assert(is_initialized_);
	return anisotropy_;
}

void OglRendererImpl::SamplerImpl::do_set_anisotropy(
	const float anisotropy)
{
	if (!is_initialized_)
	{
		assert(!"Invalid state.");
		return;
	}

	if (anisotropy < 0.0F)
	{
		assert(!"Negative max anisotropy.");
		return;
	}

	if (anisotropy_ == anisotropy)
	{
		return;
	}

	if (!has_anisotropy_)
	{
		return;
	}

	is_modified_ = true;
	ogl_renderer_.is_modified_ = true;
	is_anisotropy_modified_ = true;
	anisotropy_ = anisotropy;
}

void OglRendererImpl::SamplerImpl::set_addressing_mode_u_internal()
{
	const auto ogl_addressing_mode_u = get_ogl_addressing_mode(addressing_mode_u_);
	::glSamplerParameteri(ogl_sampler_, GL_TEXTURE_WRAP_S, ogl_addressing_mode_u);
	assert(ogl_is_succeed());
}

void OglRendererImpl::SamplerImpl::set_addressing_mode_v_internal()
{
	const auto ogl_addressing_mode_v = get_ogl_addressing_mode(addressing_mode_v_);
	::glSamplerParameteri(ogl_sampler_, GL_TEXTURE_WRAP_T, ogl_addressing_mode_v);
	assert(ogl_is_succeed());
}

void OglRendererImpl::SamplerImpl::set_mag_filter_internal()
{
	const auto mag_filter = get_ogl_mag_filter(mag_filter_);
	::glSamplerParameteri(ogl_sampler_, GL_TEXTURE_MAG_FILTER, mag_filter);
	assert(ogl_is_succeed());
}

void OglRendererImpl::SamplerImpl::set_minmip_filter_internal()
{
	const auto min_filter = get_ogl_min_filter(min_filter_, mip_filter_);
	::glSamplerParameteri(ogl_sampler_, GL_TEXTURE_MIN_FILTER, min_filter);
	assert(ogl_is_succeed());
}

void OglRendererImpl::SamplerImpl::set_lod_bias_internal()
{
	auto lod_bias = lod_bias_;

	if (lod_bias < (-max_lod_bias_))
	{
		lod_bias = -max_lod_bias_;
	}
	else if (lod_bias > (+max_lod_bias_))
	{
		lod_bias = +max_lod_bias_;
	}

	::glSamplerParameterf(ogl_sampler_, GL_TEXTURE_LOD_BIAS, lod_bias);
	assert(ogl_is_succeed());
}

void OglRendererImpl::SamplerImpl::set_anisotropy_internal()
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

void OglRendererImpl::SamplerImpl::set_defaults()
{
	is_modified_ = true;

	is_addressing_mode_u_modified_ = true;
	addressing_mode_u_ = default_addressing_mode_u;
	set_addressing_mode_u_internal();

	is_addressing_mode_v_modified_ = true;
	addressing_mode_v_ = default_addressing_mode_v;
	set_addressing_mode_v_internal();

	is_mag_filter_modified_ = true;
	mag_filter_ = default_mag_filter;
	set_mag_filter_internal();

	is_minmip_filter_modified_ = true;
	min_filter_ = default_min_filter;
	mip_filter_ = default_mip_filter;
	set_minmip_filter_internal();

	is_lod_bias_modified_ = true;
	lod_bias_ = default_lod_bias;
	set_lod_bias_internal();

	is_anisotropy_modified_ = true;
	anisotropy_ = default_max_anisotropy;
	set_anisotropy_internal();
}

void OglRendererImpl::SamplerImpl::apply_addressing_mode_u_modifications()
{
	if (!is_addressing_mode_u_modified_)
	{
		return;
	}

	is_addressing_mode_u_modified_ = false;
	set_addressing_mode_u_internal();
}

void OglRendererImpl::SamplerImpl::apply_addressing_mode_v_modifications()
{
	if (!is_addressing_mode_v_modified_)
	{
		return;
	}

	is_addressing_mode_v_modified_ = false;
	set_addressing_mode_v_internal();
}

void OglRendererImpl::SamplerImpl::apply_mag_filter_modifications()
{
	if (!is_mag_filter_modified_)
	{
		return;
	}

	is_mag_filter_modified_ = false;
	set_mag_filter_internal();
}

void OglRendererImpl::SamplerImpl::apply_minmip_filter_modifications()
{
	if (!is_minmip_filter_modified_)
	{
		return;
	}

	is_minmip_filter_modified_ = false;
	set_minmip_filter_internal();
}

void OglRendererImpl::SamplerImpl::apply_lod_bias_modifications()
{
	if (!is_lod_bias_modified_)
	{
		return;
	}

	is_lod_bias_modified_ = false;
	set_lod_bias_internal();
}

void OglRendererImpl::SamplerImpl::apply_anisotropy_modifications()
{
	if (!is_anisotropy_modified_)
	{
		return;
	}

	is_anisotropy_modified_ = false;
	set_anisotropy_internal();
}

GLenum OglRendererImpl::SamplerImpl::get_ogl_addressing_mode(
	const std::uint32_t d3d_addressing_mode)
{
	switch (d3d_addressing_mode)
	{
	case d3dtaddress_clamp:
		return GL_CLAMP_TO_EDGE;

	case d3dtaddress_wrap:
		return GL_REPEAT;

	default:
		assert(!"Unsupported texture addressing mode.");
		return invalid_ogl_enum;
	}
}

GLenum OglRendererImpl::SamplerImpl::get_ogl_mag_filter(
	const std::uint32_t d3d_mag_filter)
{
	switch (d3d_mag_filter)
	{
	case d3dtexf_point:
		return GL_NEAREST;

	case d3dtexf_linear:
	case d3dtexf_anisotropic:
		return GL_LINEAR;

	default:
		assert(!"Unsupported magnification texture filter.");
		return invalid_ogl_enum;
	}
}

GLenum OglRendererImpl::SamplerImpl::get_ogl_min_filter(
	const std::uint32_t d3d_min_filter,
	const std::uint32_t d3d_mip_filter)
{
	switch (d3d_min_filter)
	{
	case d3dtexf_point:
		switch (d3d_mip_filter)
		{
		case d3dtexf_none:
			return GL_NEAREST;

		case d3dtexf_point:
			return GL_NEAREST_MIPMAP_NEAREST;

		default:
			assert(!"Unsupported mipmap texture filter.");
			return invalid_ogl_enum;
		}
		break;

	case d3dtexf_linear:
		switch (d3d_mip_filter)
		{
		case d3dtexf_none:
			return GL_LINEAR;

		case d3dtexf_point:
			return GL_LINEAR_MIPMAP_NEAREST;

		default:
			assert(!"Unsupported mipmap texture filter.");
			return invalid_ogl_enum;
		}
		break;

	case d3dtexf_anisotropic:
		switch (d3d_mip_filter)
		{
		case d3dtexf_none:
			return GL_LINEAR;

		case d3dtexf_point:
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
// OglRendererImpl::SamplerImpl
// ==========================================================================


// ==========================================================================
// OglRenderer::Sampler
//

OglRenderer::Sampler::Sampler()
{
}

OglRenderer::Sampler::~Sampler()
{
}

std::uint32_t OglRenderer::Sampler::get_addressing_mode_u() const
{
	return do_get_addressing_mode_u();
}

void OglRenderer::Sampler::set_addressing_mode_u(
	const std::uint32_t addressing_mode_u)
{
	do_set_addressing_mode_u(addressing_mode_u);
}

std::uint32_t OglRenderer::Sampler::get_addressing_mode_v() const
{
	return do_get_addressing_mode_v();
}

void OglRenderer::Sampler::set_addressing_mode_v(
	const std::uint32_t addressing_mode_v)
{
	do_set_addressing_mode_v(addressing_mode_v);
}

std::uint32_t OglRenderer::Sampler::get_mag_filter() const
{
	return do_get_mag_filter();
}

void OglRenderer::Sampler::set_mag_filter(
	const std::uint32_t mag_filter)
{
	do_set_mag_filter(mag_filter);
}

std::uint32_t OglRenderer::Sampler::get_min_filter() const
{
	return do_get_min_filter();
}

void OglRenderer::Sampler::set_min_filter(
	const std::uint32_t min_filter)
{
	do_set_min_filter(min_filter);
}

std::uint32_t OglRenderer::Sampler::get_mip_filter() const
{
	return do_get_mip_filter();
}

void OglRenderer::Sampler::set_mip_filter(
	const std::uint32_t mip_filter)
{
	do_set_mip_filter(mip_filter);
}

float OglRenderer::Sampler::get_lod_bias() const
{
	return do_get_lod_bias();
}

void OglRenderer::Sampler::set_lod_bias(
	const float mipmap_lod_bias)
{
	do_set_lod_bias(mipmap_lod_bias);
}

float OglRenderer::Sampler::get_anisotropy() const
{
	return do_get_anisotropy();
}

void OglRenderer::Sampler::set_anisotropy(
	const float anisotropy)
{
	do_set_anisotropy(anisotropy);
}

//
// OglRenderer::Sampler
// ==========================================================================


// ==========================================================================
// OglRendererImpl::TextureImpl
//

const OglRendererImpl::TextureImpl::OglCubeMapFaceMap OglRendererImpl::TextureImpl::ogl_cube_map_face_map =
{
	GL_TEXTURE_CUBE_MAP_POSITIVE_X,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
	GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
	GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
};

OglRendererImpl::TextureImpl::TextureImpl(
	OglRendererImpl& ogl_renderer)
	:
	ogl_renderer_{ogl_renderer},
	is_initialized_{},
	ogl_texture_{},
	ogl_target_{},
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

void OglRendererImpl::TextureImpl::bind(
	const bool is_binded)
{
	bind_internal(is_binded);
}

bool OglRendererImpl::TextureImpl::do_initialize(
	const InitializeParam& param)
{
	uninitialize_internal();

#if 0
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
#endif

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
		if (param.cube_face_index_ < 0 || param.cube_face_index_ >= max_cube_faces)
		{
			assert(!"Cube face index out of range.");
			return false;
		}
	}

#if 0
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
#endif

	if (param.has_linear_filter_)
	{
		if (param.src_surface_format_ == d3dfmt_a4r4g4b4 && surface_format_ == d3dfmt_a4r4g4b4)
		{
			return upload_level_a4r4g4b4_linear(param);
		}
		else if (param.src_surface_format_ == d3dfmt_a8r8g8b8 && surface_format_ == d3dfmt_v8u8)
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

std::uint32_t OglRendererImpl::TextureImpl::do_get_surface_format() const
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
	case d3dfmt_a4r4g4b4:
	case d3dfmt_a8r8g8b8:
	case d3dfmt_v8u8:
		is_compressed_ = false;
		break;

	case d3dfmt_dxt1:
	case d3dfmt_dxt3:
	case d3dfmt_dxt5:
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
	ogl_renderer_.set_texture_unit(max_stages);

	ogl_target_ = GL_TEXTURE_2D;

	bind_internal(true);

	auto level_count = level_count_;

	if (level_count == 0)
	{
		level_count = max_level_count_;
	}

	::glTexParameteri(ogl_target_, GL_TEXTURE_MAX_LEVEL, level_count - 1);
	assert(ogl_is_succeed());

	::glTexParameteri(ogl_target_, GL_TEXTURE_WRAP_S, GL_REPEAT);
	assert(ogl_is_succeed());
	::glTexParameteri(ogl_target_, GL_TEXTURE_WRAP_T, GL_REPEAT);
	assert(ogl_is_succeed());

	if (level_count_ > 1)
	{
		::glTexParameteri(ogl_target_, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
		assert(ogl_is_succeed());
	}
	else
	{
		::glTexParameteri(ogl_target_, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		assert(ogl_is_succeed());
	}

	::glTexParameteri(ogl_target_, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	assert(ogl_is_succeed());

	const auto ogl_internal_format = get_ogl_internal_pixel_format();

	for (auto i_level = 0; i_level < level_count; ++i_level)
	{
		const auto level_width = width_ / (1 << i_level);
		const auto level_height = height_ / (1 << i_level);

		if (is_compressed_)
		{
			const auto image_size = calculate_compressed_image_size(level_width, level_height);

			::glCompressedTexImage2D(
				GL_TEXTURE_2D,
				i_level,
				ogl_internal_format,
				level_width,
				level_height,
				0, // border
				image_size,
				nullptr
			);

			assert(ogl_is_succeed());
		}
		else
		{
			::glTexImage2D(
				GL_TEXTURE_2D,
				i_level,
				ogl_internal_format,
				level_width,
				level_height,
				0, // border
				GL_RGBA,
				GL_UNSIGNED_BYTE,
				nullptr
			);

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

	ogl_renderer_.set_texture_unit(max_stages);

	ogl_target_ = GL_TEXTURE_CUBE_MAP;

	bind_internal(true);

	auto level_count = level_count_;

	if (level_count == 0)
	{
		level_count = max_level_count_;
	}

	::glTexParameteri(ogl_target_, GL_TEXTURE_MAX_LEVEL, level_count - 1);
	assert(ogl_is_succeed());

	::glTexParameteri(ogl_target_, GL_TEXTURE_WRAP_S, GL_REPEAT);
	assert(ogl_is_succeed());
	::glTexParameteri(ogl_target_, GL_TEXTURE_WRAP_T, GL_REPEAT);
	assert(ogl_is_succeed());

	if (level_count_ > 1)
	{
		::glTexParameteri(ogl_target_, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
		assert(ogl_is_succeed());
	}
	else
	{
		::glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		assert(ogl_is_succeed());
	}

	::glTexParameteri(ogl_target_, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	assert(ogl_is_succeed());

	const auto ogl_internal_format = get_ogl_internal_pixel_format();

	for (auto i_level = 0; i_level < level_count; ++i_level)
	{
		const auto level_width = width_ / (1 << i_level);
		const auto level_height = height_ / (1 << i_level);

		for (auto i_face_index = 0; i_face_index < max_cube_faces; ++i_face_index)
		{
			const auto ogl_level_target = ogl_cube_map_face_map[i_face_index];

			if (is_compressed_)
			{
				const auto image_size = calculate_compressed_image_size(level_width, level_height);

				::glCompressedTexImage2D(
					ogl_level_target,
					i_level,
					ogl_internal_format,
					level_width,
					level_height,
					0, // border
					image_size,
					nullptr
				);

				assert(ogl_is_succeed());
			}
			else
			{
				::glTexImage2D(
					ogl_level_target,
					i_level,
					ogl_internal_format,
					level_width,
					level_height,
					0, // border
					GL_RGBA,
					GL_UNSIGNED_BYTE,
					nullptr
				);

				assert(ogl_is_succeed());
			}
		}
	}

	return ogl_is_succeed();
}

void OglRendererImpl::TextureImpl::uninitialize_internal()
{
	is_initialized_ = false;

	if (ogl_texture_)
	{
		bind_internal(false);

		::glDeleteTextures(1, &ogl_texture_);
		assert(ogl_is_succeed());

		ogl_texture_ = 0;
	}

	ogl_target_ = invalid_ogl_enum;

	type_ = Type::none;
	is_compressed_ = false;
	surface_format_ = d3dfmt_unknown;
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
	case d3dfmt_a4r4g4b4:
		return GL_RGBA4;

	case d3dfmt_a8r8g8b8:
		return GL_RGBA8;

	case d3dfmt_v8u8:
		return GL_RG8_SNORM;

	case d3dfmt_dxt1:
		if (!ogl_renderer_.has_gl_ext_texture_compression_s3tc_)
		{
			return invalid_ogl_format;
		}

		return GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;

	case d3dfmt_dxt3:
		if (!ogl_renderer_.has_gl_ext_texture_compression_s3tc_)
		{
			return invalid_ogl_format;
		}

		return GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;

	case d3dfmt_dxt5:
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
	case d3dfmt_a4r4g4b4:
		format = GL_BGRA;
		type = GL_UNSIGNED_SHORT_4_4_4_4;
		break;

	case d3dfmt_a8r8g8b8:
		format = GL_BGRA;
		type = GL_UNSIGNED_BYTE;
		break;

	case d3dfmt_v8u8:
		format = GL_RG;
		type = GL_UNSIGNED_BYTE;

	case d3dfmt_dxt1:
	case d3dfmt_dxt3:
	case d3dfmt_dxt5:
		assert(!"Compressed surface formats not supported.");
		break;

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
	case d3dfmt_dxt1:
		block_size = 8;
		break;

	case d3dfmt_dxt3:
	case d3dfmt_dxt5:
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

	bind_internal(true);

	GLenum ogl_level_target;

	if (is_cube_map)
	{
		ogl_level_target = ogl_cube_map_face_map[param.cube_face_index_];
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
			0, // x
			0, // y
			level_width,
			level_height,
			ogl_format,
			image_size,
			param.raw_data_
		);

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
			0, // x
			0, // y
			level_width,
			level_height,
			ogl_format,
			ogl_type,
			param.raw_data_
		);

		assert(ogl_is_succeed());
	}

	if (param.level_ == 0 && level_count_ == 0)
	{
		::glGenerateMipmap(ogl_target_);
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
	const auto src_pitch = 2 * width;

	auto a_sum = 0;
	auto r_sum = 0;
	auto g_sum = 0;
	auto b_sum = 0;
	auto sample_count = 0;

	for (auto i = 0; i < max_interpolation_sample_count; ++i)
	{
		const auto& sample_delta = interpolation_sample_deltas[i];
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
	const auto src_pitch = 4 * width;

	auto g_sum = 0;
	auto b_sum = 0;
	auto sample_count = 0;

	for (auto i = 0; i < max_interpolation_sample_count; ++i)
	{
		const auto& sample_delta = interpolation_sample_deltas[i];
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

void OglRendererImpl::TextureImpl::bind_internal(
	const bool is_binded)
{
	auto& binding = get_binding();

	if (is_binded)
	{
		if (binding == this)
		{
			return;
		}

		binding = this;

		::glBindTexture(ogl_target_, ogl_texture_);
		assert(ogl_is_succeed());
	}
	else
	{
		if (!binding)
		{
			return;
		}

		binding = nullptr;

		::glBindTexture(ogl_target_, 0);
		assert(ogl_is_succeed());
	}
}

OglRendererImpl::TextureImpl*& OglRendererImpl::TextureImpl::get_binding()
{
	auto& current_texture_unit = ogl_renderer_.texture_units_[ogl_renderer_.current_texture_unit_index_];

	switch (type_)
	{
	case Type::two_d:
		return current_texture_unit.targets_[0];

	case Type::cube_map:
		return current_texture_unit.targets_[1];

	default:
		assert(!"Unsupported texture type.");
		throw "Unsupported texture type.";
	}
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

	if (!OglRendererImpl::is_surface_format_valid(surface_format_))
	{
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

	if (!OglRendererImpl::is_surface_format_valid(src_surface_format_))
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

bool OglRenderer::Texture::is_2d() const
{
	return do_get_type() == Type::two_d;
}

bool OglRenderer::Texture::is_cube_map() const
{
	return do_get_type() == Type::cube_map;
}

std::uint32_t OglRenderer::Texture::get_surface_format() const
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
// OglRendererImpl::StageImpl
//

OglRendererImpl::TextureImplPtr OglRendererImpl::StageImpl::default_texture = nullptr;

std::uint32_t OglRendererImpl::StageImpl::default_color_op_0 = d3dtop_modulate;
std::uint32_t OglRendererImpl::StageImpl::default_color_op_n = d3dtop_disable;
std::uint32_t OglRendererImpl::StageImpl::default_color_arg1 = d3dta_texture;
std::uint32_t OglRendererImpl::StageImpl::default_color_arg2 = d3dta_current;

std::uint32_t OglRendererImpl::StageImpl::default_alpha_op_0 = d3dtop_selectarg1;
std::uint32_t OglRendererImpl::StageImpl::default_alpha_op_n = d3dtop_disable;
std::uint32_t OglRendererImpl::StageImpl::default_alpha_arg1 = d3dta_texture;
std::uint32_t OglRendererImpl::StageImpl::default_alpha_arg2 = d3dta_current;

std::uint32_t OglRendererImpl::StageImpl::default_trans_flags = d3dttff_disable;

float OglRendererImpl::StageImpl::default_bump_map_lum_scale = 0.0F;
float OglRendererImpl::StageImpl::default_bump_map_lum_offset = 0.0F;

OglRendererImpl::Matrix2F OglRendererImpl::StageImpl::default_bump_map_matrix =
{
	0.0F, 0.0F,
	0.0F, 0.0F,
};


OglRendererImpl::StageImpl::StageImpl(
	OglRendererImpl& ogl_renderer,
	const int index)
	:
	ogl_renderer_{ogl_renderer},
	index_{index},
	is_initialized_{},
	is_modified_{},
	is_texture_modified_{},
	texture_{},
	u_is_2d_{},
	u_sampler_2d_{},
	u_sampler_cube_{},
	is_color_op_modified_{},
	color_op_{},
	u_color_op_{},
	is_color_arg1_modified_{},
	color_arg1_{},
	u_color_arg1_{},
	is_color_arg2_modified_{},
	color_arg2_{},
	u_color_arg2_{},
	is_alpha_op_modified_{},
	alpha_op_{},
	u_alpha_op_{},
	is_alpha_arg1_modified_{},
	alpha_arg1_{},
	u_alpha_arg1_{},
	is_alpha_arg2_modified_{},
	alpha_arg2_{},
	u_alpha_arg2_{},
	is_coord_index_modified_{},
	coord_index_{},
	u_coord_index_{},
	is_trans_flags_modified_{},
	trans_flags_{},
	u_trans_flags_{},
	is_bump_map_lum_scale_modified_{},
	bump_map_lum_scale_{},
	is_bump_map_lum_offset_modified_{},
	bump_map_lum_offset_{},
	is_bump_map_matrix_modified_{},
	bump_map_matrix_{}
{
}

OglRendererImpl::StageImpl::~StageImpl()
{
	uninitialize_internal();
}

bool OglRendererImpl::StageImpl::initialize()
{
	uninitialize_internal();

	if (!initialize_internal())
	{
		uninitialize_internal();
		return false;
	}

	return true;
}

void OglRendererImpl::StageImpl::uninitialize()
{
	uninitialize_internal();
}

bool OglRendererImpl::StageImpl::is_initialized() const
{
	return is_initialized_;
}

void OglRendererImpl::StageImpl::apply_modifications()
{
	if (!is_modified_)
	{
		return;
	}

	is_modified_ = false;


	apply_texture_modifications();
	apply_color_op_modifications();
	apply_color_arg1_modifications();
	apply_color_arg2_modifications();
	apply_alpha_op_modifications();
	apply_alpha_arg1_modifications();
	apply_alpha_arg2_modifications();
	apply_coord_index_modifications();
	apply_trans_flags_modifications();
	apply_bump_map_lum_scale_modifications();
	apply_bump_map_lum_offset_modifications();
	apply_bump_map_matrix_modifications();
}

OglRenderer::TexturePtr OglRendererImpl::StageImpl::do_get_texture()
{
	assert(is_initialized_);
	return texture_;
}

void OglRendererImpl::StageImpl::do_set_texture(
	TexturePtr texture)
{
	if (!is_initialized_)
	{
		assert(!"Invalid state.");
		return;
	}

	if (texture_ == texture)
	{
		return;
	}

	texture_ = static_cast<TextureImplPtr>(texture);

	ogl_renderer_.is_modified_ = true;
	is_modified_ = true;
	is_texture_modified_ = true;
}

std::uint32_t OglRendererImpl::StageImpl::do_get_color_op() const
{
	assert(is_initialized_);
	return color_op_;
}

void OglRendererImpl::StageImpl::do_set_color_op(
	const std::uint32_t color_op)
{
	if (!is_initialized_)
	{
		assert(!"Invalid state.");
		return;
	}

	if (!is_op_valid(color_op))
	{
		return;
	}

	if (color_op_ == color_op)
	{
		return;
	}

	color_op_ = color_op;

	ogl_renderer_.is_modified_ = true;
	is_modified_ = true;
	is_color_op_modified_ = true;
}

std::uint32_t OglRendererImpl::StageImpl::do_get_color_arg1() const
{
	assert(is_initialized_);
	return color_arg1_;
}

void OglRendererImpl::StageImpl::do_set_color_arg1(
	const std::uint32_t arg1)
{
	if (!is_initialized_)
	{
		assert(!"Invalid state.");
		return;
	}

	if (!is_arg_valid(arg1))
	{
		return;
	}

	if (color_arg1_ == arg1)
	{
		return;
	}

	color_arg1_ = arg1;

	ogl_renderer_.is_modified_ = true;
	is_modified_ = true;
	is_color_arg1_modified_ = true;
}

std::uint32_t OglRendererImpl::StageImpl::do_get_color_arg2() const
{
	assert(is_initialized_);
	return color_arg2_;
}

void OglRendererImpl::StageImpl::do_set_color_arg2(
	const std::uint32_t arg2)
{
	if (!is_initialized_)
	{
		assert(!"Invalid state.");
		return;
	}

	if (!is_arg_valid(arg2))
	{
		return;
	}

	if (color_arg2_ == arg2)
	{
		return;
	}

	color_arg2_ = arg2;

	ogl_renderer_.is_modified_ = true;
	is_modified_ = true;
	is_color_arg2_modified_ = true;
}

std::uint32_t OglRendererImpl::StageImpl::do_get_alpha_op() const
{
	assert(is_initialized_);
	return alpha_op_;
}

void OglRendererImpl::StageImpl::do_set_alpha_op(
	const std::uint32_t alpha_op)
{
	if (!is_initialized_)
	{
		assert(!"Invalid state.");
		return;
	}

	if (!is_op_valid(alpha_op))
	{
		return;
	}

	if (alpha_op_ == alpha_op)
	{
		return;
	}

	alpha_op_ = alpha_op;

	ogl_renderer_.is_modified_ = true;
	is_modified_ = true;
	is_alpha_op_modified_ = true;
}

std::uint32_t OglRendererImpl::StageImpl::do_get_alpha_arg1() const
{
	assert(is_initialized_);
	return alpha_arg1_;
}

void OglRendererImpl::StageImpl::do_set_alpha_arg1(
	const std::uint32_t alpha_arg1)
{
	if (!is_initialized_)
	{
		assert(!"Invalid state.");
		return;
	}

	if (!is_arg_valid(alpha_arg1))
	{
		return;
	}

	if (alpha_arg1_ == alpha_arg1)
	{
		return;
	}

	alpha_arg1_ = alpha_arg1;

	ogl_renderer_.is_modified_ = true;
	is_modified_ = true;
	is_alpha_arg1_modified_ = true;
}

std::uint32_t OglRendererImpl::StageImpl::do_get_alpha_arg2() const
{
	assert(is_initialized_);
	return alpha_arg2_;
}

void OglRendererImpl::StageImpl::do_set_alpha_arg2(
	const std::uint32_t alpha_arg2)
{
	if (!is_initialized_)
	{
		assert(!"Invalid state.");
		return;
	}

	if (!is_arg_valid(alpha_arg2))
	{
		return;
	}

	if (alpha_arg2_ == alpha_arg2)
	{
		return;
	}

	alpha_arg2_ = alpha_arg2;

	ogl_renderer_.is_modified_ = true;
	is_modified_ = true;
	is_alpha_arg2_modified_ = true;
}

std::uint32_t OglRendererImpl::StageImpl::do_get_coord_index() const
{
	assert(is_initialized_);
	return coord_index_;
}

void OglRendererImpl::StageImpl::do_set_coord_index(
	const std::uint32_t coord_index)
{
	if (!is_initialized_)
	{
		assert(!"Invalid state.");
		return;
	}

	if (!is_coord_index_valid(coord_index))
	{
		return;
	}

	if (coord_index_ == coord_index)
	{
		return;
	}

	coord_index_ = coord_index;

	ogl_renderer_.is_modified_ = true;
	is_modified_ = true;
	is_coord_index_modified_ = true;
}

std::uint32_t OglRendererImpl::StageImpl::do_get_trans_flags() const
{
	assert(is_initialized_);
	return trans_flags_;
}

void OglRendererImpl::StageImpl::do_set_trans_flags(
	const std::uint32_t trans_flags)
{
	if (!is_initialized_)
	{
		assert(!"Invalid state.");
		return;
	}

	if (!are_trans_flags_valid(trans_flags))
	{
		return;
	}

	if (trans_flags_ == trans_flags)
	{
		return;
	}

	trans_flags_ = trans_flags;

	ogl_renderer_.is_modified_ = true;
	is_modified_ = true;
	is_trans_flags_modified_ = true;
}

float OglRendererImpl::StageImpl::do_get_bump_map_lum_scale() const
{
	assert(is_initialized_);
	return bump_map_lum_scale_;
}

void OglRendererImpl::StageImpl::do_set_bump_map_lum_scale(
	const float lum_scale)
{
	if (!is_initialized_)
	{
		assert(!"Invalid state.");
		return;
	}

	if (bump_map_lum_scale_ == lum_scale)
	{
		return;
	}

	bump_map_lum_scale_ = lum_scale;

	ogl_renderer_.is_modified_ = true;
	is_modified_ = true;
	is_bump_map_lum_scale_modified_ = true;
}

float OglRendererImpl::StageImpl::do_get_bump_map_lum_offset() const
{
	assert(is_initialized_);
	return bump_map_lum_offset_;
}

void OglRendererImpl::StageImpl::do_set_bump_map_lum_offset(
	const float lum_offset)
{
	if (!is_initialized_)
	{
		assert(!"Invalid state.");
		return;
	}

	if (bump_map_lum_offset_ == lum_offset)
	{
		return;
	}

	bump_map_lum_offset_ = lum_offset;

	ogl_renderer_.is_modified_ = true;
	is_modified_ = true;
	is_bump_map_lum_offset_modified_ = true;
}

float OglRendererImpl::StageImpl::do_get_bump_map_coefficient(
	const int index) const
{
	assert(is_initialized_);
	return bump_map_matrix_[index];
}

void OglRendererImpl::StageImpl::do_set_bump_map_coefficient(
	const int index,
	const float coefficient)
{
	if (!is_initialized_)
	{
		assert(!"Invalid state.");
		return;
	}

	if (bump_map_matrix_[index] == coefficient)
	{
		return;
	}

	bump_map_matrix_[index] = coefficient;

	ogl_renderer_.is_modified_ = true;
	is_modified_ = true;
	is_bump_map_matrix_modified_ = true;
}

bool OglRendererImpl::StageImpl::initialize_internal()
{
	locate_uniforms();

	set_defaults();

	is_initialized_ = true;

	return true;
}

void OglRendererImpl::StageImpl::uninitialize_internal()
{
	ogl_renderer_.set_texture_for_unit(index_, nullptr);

	is_initialized_ = false;
	is_modified_ = false;

	is_texture_modified_ = false;
	texture_ = nullptr;
	u_is_2d_ = -1;
	u_sampler_2d_ = -1;
	u_sampler_cube_ = -1;

	is_color_op_modified_ = false;
	color_op_ = 0;
	u_color_op_ = -1;

	is_color_arg1_modified_ = false;
	color_arg1_ = 0;
	u_color_arg1_ = -1;

	is_color_arg2_modified_ = false;
	color_arg2_ = 0;
	u_color_arg2_ = -1;

	is_alpha_op_modified_ = false;
	alpha_op_ = 0;
	u_alpha_op_ = -1;

	is_alpha_arg1_modified_ = false;
	alpha_arg1_ = 0;
	u_alpha_arg1_ = -1;

	is_alpha_arg2_modified_ = false;
	alpha_arg2_ = 0;
	u_alpha_arg2_ = -1;

	is_coord_index_modified_ = false;
	coord_index_ = 0;
	u_coord_index_ = -1;

	is_trans_flags_modified_ = false;
	trans_flags_ = 0;
	u_trans_flags_ = -1;

	is_bump_map_lum_scale_modified_ = false;
	bump_map_lum_scale_ = 0.0F;

	is_bump_map_lum_offset_modified_ = false;
	bump_map_lum_offset_ = 0.0F;

	is_bump_map_matrix_modified_ = false;
	bump_map_matrix_.fill(0.0F);
}

void OglRendererImpl::StageImpl::set_defaults()
{
	is_modified_ = true;

	is_texture_modified_ = true;
	texture_ = default_texture;
	set_texture_internal();

	is_color_op_modified_ = true;
	color_op_ = (index_ == 0 ? default_color_op_0 : default_color_op_n);
	set_color_op_internal();

	is_color_arg1_modified_ = true;
	color_arg1_ = default_color_arg1;
	set_color_arg1_internal();

	is_color_arg2_modified_ = true;
	color_arg2_ = default_color_arg2;
	set_color_arg2_internal();

	is_alpha_op_modified_ = true;
	alpha_op_ = (index_ == 0 ? default_alpha_op_0 : default_alpha_op_n);
	set_alpha_op_internal();

	is_alpha_arg1_modified_ = true;
	alpha_arg1_ = default_alpha_arg1;
	set_alpha_arg1_internal();

	is_alpha_arg2_modified_ = true;
	alpha_arg2_ = default_alpha_arg2;
	set_alpha_arg2_internal();

	is_coord_index_modified_ = true;
	coord_index_ = index_;
	set_coord_index_internal();

	is_trans_flags_modified_ = true;
	trans_flags_ = default_trans_flags;
	set_trans_flags_internal();

	is_bump_map_lum_scale_modified_ = true;
	bump_map_lum_scale_ = default_bump_map_lum_scale;
	set_bump_map_lum_scale_internal();

	is_bump_map_lum_offset_modified_ = true;
	bump_map_lum_offset_ = default_bump_map_lum_offset;
	set_bump_map_lum_offset_internal();

	is_bump_map_matrix_modified_ = true;
	bump_map_matrix_ = default_bump_map_matrix;
	set_bump_map_matrix_internal();


	::glUniform1i(u_sampler_2d_, index_);
	assert(ogl_is_succeed());

	::glUniform1i(u_sampler_cube_, index_);
	assert(ogl_is_succeed());
}

void OglRendererImpl::StageImpl::locate_uniforms()
{
	const auto index_string = std::to_string(index_);

	u_sampler_2d_ = ogl_renderer_.locate_uniform("u_samplers_2d[" + index_string + "]");
	u_sampler_cube_ = ogl_renderer_.locate_uniform("u_samplers_cube[" + index_string + "]");


	const auto base_name = "u_stages[" + index_string + "].";

	u_is_2d_ = ogl_renderer_.locate_uniform(base_name + "is_2d");

	u_color_op_ = ogl_renderer_.locate_uniform(base_name + "color_op");
	u_color_arg1_ = ogl_renderer_.locate_uniform(base_name + "color_arg1");
	u_color_arg2_ = ogl_renderer_.locate_uniform(base_name + "color_arg2");

	u_alpha_op_ = ogl_renderer_.locate_uniform(base_name + "alpha_op");
	u_alpha_arg1_ = ogl_renderer_.locate_uniform(base_name + "alpha_arg1");
	u_alpha_arg2_ = ogl_renderer_.locate_uniform(base_name + "alpha_arg2");

	u_coord_index_ = ogl_renderer_.locate_uniform(base_name + "coord_index");
	u_trans_flags_ = ogl_renderer_.locate_uniform(base_name + "trans_flags");
}

void OglRendererImpl::StageImpl::set_texture_internal()
{
	ogl_renderer_.set_texture_for_unit(index_, texture_);

	auto is_2d = true;

	if (texture_ && texture_->is_cube_map())
	{
		is_2d = false;
	}

	::glUniform1i(u_is_2d_, is_2d);
	assert(ogl_is_succeed());
}

void OglRendererImpl::StageImpl::set_color_op_internal()
{
	::glUniform1ui(u_color_op_, static_cast<GLuint>(color_op_));
	assert(ogl_is_succeed());
}

void OglRendererImpl::StageImpl::set_color_arg1_internal()
{
	::glUniform1ui(u_color_arg1_, static_cast<GLuint>(color_arg1_));
	assert(ogl_is_succeed());
}

void OglRendererImpl::StageImpl::set_color_arg2_internal()
{
	::glUniform1ui(u_color_arg2_, static_cast<GLuint>(color_arg2_));
	assert(ogl_is_succeed());
}

void OglRendererImpl::StageImpl::set_alpha_op_internal()
{
	::glUniform1ui(u_alpha_op_, static_cast<GLuint>(alpha_op_));
	assert(ogl_is_succeed());
}

void OglRendererImpl::StageImpl::set_alpha_arg1_internal()
{
	::glUniform1ui(u_alpha_arg1_, static_cast<GLuint>(alpha_arg1_));
	assert(ogl_is_succeed());
}

void OglRendererImpl::StageImpl::set_alpha_arg2_internal()
{
	::glUniform1ui(u_alpha_arg2_, static_cast<GLuint>(alpha_arg2_));
	assert(ogl_is_succeed());
}

void OglRendererImpl::StageImpl::set_coord_index_internal()
{
	::glUniform1ui(u_coord_index_, static_cast<GLuint>(coord_index_));
	assert(ogl_is_succeed());
}

void OglRendererImpl::StageImpl::set_trans_flags_internal()
{
	::glUniform1ui(u_trans_flags_, static_cast<GLuint>(trans_flags_));
	assert(ogl_is_succeed());
}

void OglRendererImpl::StageImpl::set_bump_map_lum_scale_internal()
{
	//assert(!"Not implemented.");
}

void OglRendererImpl::StageImpl::set_bump_map_lum_offset_internal()
{
	//assert(!"Not implemented.");
}

void OglRendererImpl::StageImpl::set_bump_map_matrix_internal()
{
	//assert(!"Not implemented.");
}

void OglRendererImpl::StageImpl::apply_texture_modifications()
{
	if (!is_texture_modified_)
	{
		return;
	}

	is_texture_modified_ = false;

	set_texture_internal();
}

void OglRendererImpl::StageImpl::apply_color_op_modifications()
{
	if (!is_color_op_modified_)
	{
		return;
	}

	is_color_op_modified_ = false;

	set_color_op_internal();
}

void OglRendererImpl::StageImpl::apply_color_arg1_modifications()
{
	if (!is_color_arg1_modified_)
	{
		return;
	}

	is_color_arg1_modified_ = false;

	set_color_arg1_internal();
}

void OglRendererImpl::StageImpl::apply_color_arg2_modifications()
{
	if (!is_color_arg2_modified_)
	{
		return;
	}

	is_color_arg2_modified_ = false;

	set_color_arg2_internal();
}

void OglRendererImpl::StageImpl::apply_alpha_op_modifications()
{
	if (!is_alpha_op_modified_)
	{
		return;
	}

	is_alpha_op_modified_ = false;

	set_alpha_op_internal();
}

void OglRendererImpl::StageImpl::apply_alpha_arg1_modifications()
{
	if (!is_alpha_arg1_modified_)
	{
		return;
	}

	is_alpha_arg1_modified_ = false;

	set_alpha_arg1_internal();
}

void OglRendererImpl::StageImpl::apply_alpha_arg2_modifications()
{
	if (!is_alpha_arg2_modified_)
	{
		return;
	}

	is_alpha_arg2_modified_ = false;

	set_alpha_arg2_internal();
}

void OglRendererImpl::StageImpl::apply_coord_index_modifications()
{
	if (!is_coord_index_modified_)
	{
		return;
	}

	is_coord_index_modified_ = false;

	set_coord_index_internal();
}

void OglRendererImpl::StageImpl::apply_trans_flags_modifications()
{
	if (!is_trans_flags_modified_)
	{
		return;
	}

	is_trans_flags_modified_ = false;

	set_trans_flags_internal();
}

void OglRendererImpl::StageImpl::apply_bump_map_lum_scale_modifications()
{
	if (!is_bump_map_lum_scale_modified_)
	{
		return;
	}

	is_bump_map_lum_scale_modified_ = false;

	set_bump_map_lum_scale_internal();
}

void OglRendererImpl::StageImpl::apply_bump_map_lum_offset_modifications()
{
	if (!is_bump_map_lum_offset_modified_)
	{
		return;
	}

	is_bump_map_lum_offset_modified_ = false;

	set_bump_map_lum_offset_internal();
}

void OglRendererImpl::StageImpl::apply_bump_map_matrix_modifications()
{
	if (!is_bump_map_matrix_modified_)
	{
		return;
	}

	is_bump_map_matrix_modified_ = false;

	set_bump_map_matrix_internal();
}

bool OglRendererImpl::StageImpl::is_op_valid(
	const std::uint32_t op)
{
	switch (op)
	{
	case d3dtop_disable:
	case d3dtop_selectarg1:
	case d3dtop_selectarg2:
	case d3dtop_modulate:
	case d3dtop_modulate2x:
	case d3dtop_add:
	case d3dtop_addsigned:
	case d3dtop_subtract:
	case d3dtop_blendcurrentalpha:
	case d3dtop_modulatealpha_addcolor:
	case d3dtop_bumpenvmap:
	case d3dtop_bumpenvmapluminance:
	case d3dtop_dotproduct3:
		return true;

	default:
		assert(!"Unsupported operation.");
		return false;
	}
}

bool OglRendererImpl::StageImpl::is_arg_valid(
	const std::uint32_t arg)
{
	const auto value_mask = 0xF;
	const auto flags_mask = 0xFFFFFFFF ^ value_mask;

	const auto value = static_cast<std::uint32_t>(arg & value_mask);
	const auto flags = arg & flags_mask;

	switch (value)
	{
	case d3dta_diffuse:
	case d3dta_current:
	case d3dta_texture:
	case d3dta_tfactor:
		break;

	default:
		assert(!"Unsupported argument.");
		return false;
	}

	switch (flags)
	{
	case 0:
	case d3dta_complement:
		break;

	default:
		assert(!"Unsupported flag.");
		return false;
	}

	return true;
}

bool OglRendererImpl::StageImpl::are_trans_flags_valid(
	const std::uint32_t trans_flags)
{
	const auto value_mask = 0xFF;
	const auto flags_mask = 0xFFFFFFFF ^ value_mask;

	const auto value = trans_flags & value_mask;
	const auto flags = trans_flags & flags_mask;

	switch (value)
	{
	case d3dttff_disable:
	case d3dttff_count2:
	case d3dttff_count3:
		break;

	default:
		assert(!"Unsupported value.");
		return false;
	}

	switch (flags)
	{
	case 0:
	case d3dttff_projected:
		break;

	default:
		assert(!"Unsupported flag.");
		return false;
	}

	return true;
}

bool OglRendererImpl::StageImpl::is_coord_index_valid(
	const std::uint32_t coord_index)
{
	const auto value_mask = 0xFFFF;
	const auto flags_mask = 0xFFFFFFFF ^ value_mask;

	const auto value = coord_index & value_mask;
	const auto flags = coord_index & flags_mask;

	if (value >= max_stages)
	{
		assert(!"Coord index out of range.");
		return false;
	}

	switch (flags)
	{
	case d3dtss_tci_passthru:
	case d3dtss_tci_cameraspaceposition:
	case d3dtss_tci_cameraspacereflectionvector:
		break;

	default:
		assert(!"Unsupported flag.");
		return false;
	}

	return true;
}

//
// OglRendererImpl::StageImpl
// ==========================================================================


// ==========================================================================
// OglRenderer::Stage
//

OglRenderer::Stage::Stage()
{
}

OglRenderer::Stage::~Stage()
{
}

OglRenderer::TexturePtr OglRenderer::Stage::get_texture()
{
	return do_get_texture();
}

void OglRenderer::Stage::set_texture(
	TexturePtr texture)
{
	do_set_texture(texture);
}

std::uint32_t OglRenderer::Stage::get_color_op() const
{
	return do_get_color_op();
}

void OglRenderer::Stage::set_color_op(
	const std::uint32_t operation)
{
	do_set_color_op(operation);
}

std::uint32_t OglRenderer::Stage::get_color_arg1() const
{
	return do_get_color_arg1();
}

void OglRenderer::Stage::set_color_arg1(
	const std::uint32_t argument_1)
{
	do_set_color_arg1(argument_1);
}

std::uint32_t OglRenderer::Stage::get_color_arg2() const
{
	return do_get_color_arg2();
}

void OglRenderer::Stage::set_color_arg2(
	const std::uint32_t argument_2)
{
	do_set_color_arg2(argument_2);
}

std::uint32_t OglRenderer::Stage::get_alpha_op() const
{
	return do_get_alpha_op();
}

void OglRenderer::Stage::set_alpha_op(
	const std::uint32_t operation)
{
	do_set_alpha_op(operation);
}

std::uint32_t OglRenderer::Stage::get_alpha_arg1() const
{
	return do_get_alpha_arg1();
}

void OglRenderer::Stage::set_alpha_arg1(
	const std::uint32_t argument_1)
{
	do_set_alpha_arg1(argument_1);
}

std::uint32_t OglRenderer::Stage::get_alpha_arg2() const
{
	return do_get_alpha_arg2();
}

void OglRenderer::Stage::set_alpha_arg2(
	const std::uint32_t argument_2)
{
	do_set_alpha_arg2(argument_2);
}

std::uint32_t OglRenderer::Stage::get_coord_index() const
{
	return do_get_coord_index();
}

void OglRenderer::Stage::set_coord_index(
	const std::uint32_t index)
{
	do_set_coord_index(index);
}

std::uint32_t OglRenderer::Stage::get_trans_flags() const
{
	return do_get_trans_flags();
}

void OglRenderer::Stage::set_trans_flags(
	const std::uint32_t flags)
{
	do_set_trans_flags(flags);
}

float OglRenderer::Stage::get_bump_map_lum_scale() const
{
	return do_get_bump_map_lum_scale();
}

void OglRenderer::Stage::set_bump_map_lum_scale(
	const float scale)
{
	do_set_bump_map_lum_scale(scale);
}

float OglRenderer::Stage::get_bump_map_lum_offset() const
{
	return do_get_bump_map_lum_offset();
}

void OglRenderer::Stage::set_bump_map_lum_offset(
	const float offset)
{
	do_set_bump_map_lum_offset(offset);
}

float OglRenderer::Stage::get_bump_map_coefficient(
	const int index) const
{
	return do_get_bump_map_coefficient(index);
}

void OglRenderer::Stage::set_bump_map_coefficient(
	const int index,
	const float coefficient)
{
	do_set_bump_map_coefficient(index, coefficient);
}

//
// OglRenderer::Stage
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
	static_assert(class_size == static_cast<int>(sizeof(Viewport)), "Class size mismatch.");
}

//
// OglRenderer::Viewport
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
#if 0
	do_set_is_current_context(true);
#endif
	do_uninitialize();
}

#if 0
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
#endif

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
	const std::uint32_t clear_flags)
{
	do_clear(clear_flags);
}

void OglRenderer::set_viewport(
	const Viewport& viewport)
{
	do_set_viewport(viewport);
}

std::uint32_t OglRenderer::get_cull_mode() const
{
	return do_get_cull_mode();
}

void OglRenderer::set_cull_mode(
	const std::uint32_t cull_mode)
{
	do_set_cull_mode(cull_mode);
}

std::uint32_t OglRenderer::get_fill_mode() const
{
	return do_get_fill_mode();
}

void OglRenderer::set_fill_mode(
	const std::uint32_t fill_mode)
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

std::uint32_t OglRenderer::get_depth_func() const
{
	return do_get_depth_func();
}

void OglRenderer::set_depth_func(
	const std::uint32_t depth_func)
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

std::uint32_t OglRenderer::get_src_blending_factor() const
{
	return do_get_src_blending_factor();
}

std::uint32_t OglRenderer::get_dst_blending_factor() const
{
	return do_get_dst_blending_factor();
}

void OglRenderer::set_blending_factors(
	const std::uint32_t src_blending_function,
	const std::uint32_t dst_blending_function)
{
	do_set_blending_factors(src_blending_function, dst_blending_function);
}

bool OglRenderer::get_is_alpha_test_enabled() const
{
	return do_get_is_alpha_test_enabled();
}

void OglRenderer::set_is_alpha_test_enabled(
	const bool is_alpha_test_enabled)
{
	do_set_is_alpha_test_enabled(is_alpha_test_enabled);
}

std::uint32_t OglRenderer::get_alpha_test_func() const
{
	return do_get_alpha_test_func();
}

void OglRenderer::set_alpha_test_func(
	const std::uint32_t d3d9_alpha_test_func)
{
	do_set_alpha_test_func(d3d9_alpha_test_func);
}

std::uint32_t OglRenderer::get_alpha_test_ref() const
{
	return do_get_alpha_test_ref();
}

void OglRenderer::set_alpha_test_ref(
	const std::uint32_t d3d9_alpha_test_ref)
{
	do_set_alpha_test_ref(d3d9_alpha_test_ref);
}

std::uint32_t OglRenderer::get_texture_factor() const
{
	return do_get_texture_factor();
}

void OglRenderer::set_texture_factor(
	const std::uint32_t d3d_texture_factor)
{
	do_set_texture_factor(d3d_texture_factor);
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

void OglRenderer::set_world_matrix(
	const int index,
	const float (&world_matrix)[16])
{
	do_set_world_matrix(index, world_matrix);
}

void OglRenderer::set_world_matrix(
	const int index,
	const float (&world_matrix)[4][4])
{
	do_set_world_matrix(index, reinterpret_cast<const float*>(world_matrix));
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

void OglRenderer::set_view_matrix(
	const float (&view_matrix)[16])
{
	do_set_view_matrix(view_matrix);
}

void OglRenderer::set_view_matrix(
	const float (&view_matrix)[4][4])
{
	do_set_view_matrix(reinterpret_cast<const float*>(view_matrix));
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

void OglRenderer::set_projection_matrix(
	const float (&projection_matrix)[16])
{
	do_set_projection_matrix(projection_matrix);
}

void OglRenderer::set_projection_matrix(
	const float (&projection_matrix)[4][4])
{
	do_set_projection_matrix(reinterpret_cast<const float*>(projection_matrix));
}

const float* OglRenderer::get_texture_matrix(
	const int index) const
{
	return do_get_texture_matrix(index);
}

void OglRenderer::set_texture_matrix(
	const int index,
	const float* const texture_matrix_ptr)
{
	do_set_texture_matrix(index, texture_matrix_ptr);
}

void OglRenderer::set_texture_matrix(
	const int index,
	const float (&texture_matrix)[16])
{
	do_set_texture_matrix(index, texture_matrix);
}

void OglRenderer::set_texture_matrix(
	const int index,
	const float (&texture_matrix)[4][4])
{
	do_set_texture_matrix(index, reinterpret_cast<const float*>(texture_matrix));
}

OglRenderer::SamplerPtr OglRenderer::get_sampler(
	const int index)
{
	return do_get_sampler(index);
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

OglRenderer::Stage& OglRenderer::get_stage(
	const int stage_index)
{
	return do_get_stage(stage_index);
}

void OglRenderer::draw(
	const std::uint32_t primitive_type,
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
