#ifndef LTJS_OGL_RENDERER_INCLUDED
#define LTJS_OGL_RENDERER_INCLUDED


#include <cstdint>
#include <array>
#include "bibendovsky_spul_enum_flags.h"
#include "bibendovsky_spul_four_cc.h"


namespace ltjs
{


namespace ul = bibendovsky::spul;


class OglRenderer
{
public:
	static constexpr auto max_stages = 4;


	//
	// Direct3D 9 clear flags.
	//

	static constexpr std::uint32_t d3dclear_target = 0x00000001;
	static constexpr std::uint32_t d3dclear_zbuffer = 0x00000002;
	static constexpr std::uint32_t d3dclear_stencil = 0x00000004;


	//
	// Direct3D 9 cull mode.
	//

	static constexpr std::uint32_t d3dcull_none = 1;
	static constexpr std::uint32_t d3dcull_cw = 2;
	static constexpr std::uint32_t d3dcull_ccw = 3;

	//
	// Direct3D 9 fill mode.
	//

	static constexpr std::uint32_t d3dfill_wireframe = 2;
	static constexpr std::uint32_t d3dfill_solid = 3;

	//
	// Direct3D 9 compare functions.
	//

	static constexpr std::uint32_t d3dcmp_less = 2;
	static constexpr std::uint32_t d3dcmp_equal = 3;
	static constexpr std::uint32_t d3dcmp_lessequal = 4;
	static constexpr std::uint32_t d3dcmp_greater = 5;
	static constexpr std::uint32_t d3dcmp_notequal = 6;
	static constexpr std::uint32_t d3dcmp_greaterequal = 7;
	static constexpr std::uint32_t d3dcmp_always = 8;

	//
	// Direct3D 9 blending factors.
	//

	static constexpr std::uint32_t d3dblend_zero = 1;
	static constexpr std::uint32_t d3dblend_one = 2;
	static constexpr std::uint32_t d3dblend_srccolor = 3;
	static constexpr std::uint32_t d3dblend_invsrccolor = 4;
	static constexpr std::uint32_t d3dblend_srcalpha = 5;
	static constexpr std::uint32_t d3dblend_invsrcalpha = 6;
	static constexpr std::uint32_t d3dblend_destcolor = 9;
	static constexpr std::uint32_t d3dblend_invdestcolor = 10;


	//
	// Direct3D 9 primitive types.
	//

	static constexpr std::uint32_t d3dpt_trianglelist = 4;
	static constexpr std::uint32_t d3dpt_trianglestrip = 5;
	static constexpr std::uint32_t d3dpt_trianglefan = 6;


	//
	// Direct3D 9 texture addressing modes.
	//

	static constexpr std::uint32_t d3dtaddress_wrap = 1;
	static constexpr std::uint32_t d3dtaddress_clamp = 3;


	//
	// Direct3D 9 texture filters.
	//

	static constexpr std::uint32_t d3dtexf_none = 0;
	static constexpr std::uint32_t d3dtexf_point = 1;
	static constexpr std::uint32_t d3dtexf_linear = 2;
	static constexpr std::uint32_t d3dtexf_anisotropic = 3;


	//
	// Direct3D 9 surface formats.
	//

	static constexpr std::uint32_t d3dfmt_unknown = 0;
	static constexpr std::uint32_t d3dfmt_a8r8g8b8 = 21;
	static constexpr std::uint32_t d3dfmt_a4r4g4b4 = 26;
	static constexpr std::uint32_t d3dfmt_v8u8 = 60;
	static constexpr std::uint32_t d3dfmt_dxt1 = ul::FourCc{"DXT1"};
	static constexpr std::uint32_t d3dfmt_dxt3 = ul::FourCc{"DXT3"};
	static constexpr std::uint32_t d3dfmt_dxt5 = ul::FourCc{"DXT5"};


	//
	// Direct3D 9 texture stage state values.
	//

	static constexpr std::uint32_t d3dtss_colorop = 1;
	static constexpr std::uint32_t d3dtss_colorarg1 = 2;
	static constexpr std::uint32_t d3dtss_colorarg2 = 3;
	static constexpr std::uint32_t d3dtss_alphaop = 4;
	static constexpr std::uint32_t d3dtss_alphaarg1 = 5;
	static constexpr std::uint32_t d3dtss_alphaarg2 = 6;
	static constexpr std::uint32_t d3dtss_bumpenvmat00 = 7;
	static constexpr std::uint32_t d3dtss_bumpenvmat01 = 8;
	static constexpr std::uint32_t d3dtss_bumpenvmat10 = 9;
	static constexpr std::uint32_t d3dtss_bumpenvmat11 = 10;
	static constexpr std::uint32_t d3dtss_texcoordindex = 11;
	static constexpr std::uint32_t d3dtss_bumpenvlscale = 22;
	static constexpr std::uint32_t d3dtss_bumpenvloffset = 23;
	static constexpr std::uint32_t d3dtss_texturetransformflags = 24;


	//
	// Direct3D 9 texture coordinate index flags.
	//

	static constexpr std::uint32_t d3dtss_tci_passthru = 0x00000000;
	static constexpr std::uint32_t d3dtss_tci_cameraspaceposition = 0x00020000;
	static constexpr std::uint32_t d3dtss_tci_cameraspacereflectionvector = 0x00030000;


	//
	// Direct3D 9 texture operation values.
	//

	static constexpr std::uint32_t d3dtop_disable = 1;
	static constexpr std::uint32_t d3dtop_selectarg1 = 2;
	static constexpr std::uint32_t d3dtop_selectarg2 = 3;
	static constexpr std::uint32_t d3dtop_modulate = 4;
	static constexpr std::uint32_t d3dtop_modulate2x = 5;
	static constexpr std::uint32_t d3dtop_add = 7;
	static constexpr std::uint32_t d3dtop_addsigned = 8;
	static constexpr std::uint32_t d3dtop_subtract = 10;
	static constexpr std::uint32_t d3dtop_blendcurrentalpha = 16;
	static constexpr std::uint32_t d3dtop_modulatealpha_addcolor = 18;
	static constexpr std::uint32_t d3dtop_bumpenvmap = 22;
	static constexpr std::uint32_t d3dtop_bumpenvmapluminance = 23;
	static constexpr std::uint32_t d3dtop_dotproduct3 = 24;


	//
	// Direct3D 9 texture argument values.
	//

	static constexpr std::uint32_t d3dta_diffuse = 0x00000000;
	static constexpr std::uint32_t d3dta_current = 0x00000001;
	static constexpr std::uint32_t d3dta_texture = 0x00000002;
	static constexpr std::uint32_t d3dta_tfactor = 0x00000003;
	static constexpr std::uint32_t d3dta_complement = 0x00000010;


	//
	// Direct3D 9 texture transformation values.
	//

	static constexpr std::uint32_t d3dttff_disable = 0;
	static constexpr std::uint32_t d3dttff_count2 = 2;
	static constexpr std::uint32_t d3dttff_count3 = 3;
	static constexpr std::uint32_t d3dttff_projected = 256;


	//
	// Direct3D 9 usage flags.
	//

	static constexpr std::uint32_t d3dusage_writeonly = 0x00000008;
	static constexpr std::uint32_t d3dusage_dynamic = 0x00000200;


	//
	// Direct3D 9 flexible vertex formats.
	//
	static constexpr std::uint32_t d3dfvf_xyz = 0x002;
	static constexpr std::uint32_t d3dfvf_xyzrhw = 0x004;
	static constexpr std::uint32_t d3dfvf_xyzb1 = 0x006;
	static constexpr std::uint32_t d3dfvf_xyzb2 = 0x008;
	static constexpr std::uint32_t d3dfvf_xyzb3 = 0x00A;
	static constexpr std::uint32_t d3dfvf_normal = 0x010;
	static constexpr std::uint32_t d3dfvf_diffuse = 0x040;
	static constexpr std::uint32_t d3dfvf_tex1 = 0x100;
	static constexpr std::uint32_t d3dfvf_tex2 = 0x200;
	static constexpr std::uint32_t d3dfvf_tex3 = 0x300;
	static constexpr std::uint32_t d3dfvf_tex4 = 0x400;
	static constexpr std::uint32_t d3dfvf_textureformat2 = 0;
	static constexpr std::uint32_t d3dfvf_textureformat3 = 1;
	static constexpr std::uint32_t d3dfvf_textureformat4 = 2;

	static constexpr std::uint32_t d3dfvf_texcoordsize2(
		const int index)
	{
		return d3dfvf_textureformat2;
	}

	static constexpr std::uint32_t d3dfvf_texcoordsize3(
		const int index)
	{
		return d3dfvf_textureformat3 << ((index * 2) + 16);
	}

	static constexpr std::uint32_t d3dfvf_texcoordsize4(
		const int index)
	{
		return d3dfvf_textureformat4 << ((index * 2) + 16);
	}


	struct Viewport
	{
		int x_;
		int y_;
		int width_;
		int height_;

		float depth_min_z_;
		float depth_max_z_;


		Viewport();
	}; // Viewport


	class VertexArrayObject
	{
	public:
		struct InitializeParam
		{
			bool has_index_;
			std::uint32_t fvf_;
			std::uint32_t vertex_usage_flags_;
			int vertex_count_;
			const void* raw_vertex_data_;
			std::uint32_t index_usage_flags_;
			int index_count_;
			const void* raw_index_data_;


			InitializeParam();

			bool is_valid() const;
		}; // InitializeParam


		bool initialize(
			const InitializeParam& param);

		void uninitialize();


		void set_vertex_data(
			const int vertex_count,
			const void* const raw_data);

		void set_vertex_data(
			const int vertex_index,
			const int vertex_count,
			const void* const raw_data);

		void draw(
			const std::uint32_t primitive_type,
			const int primitive_count);

		void draw(
			const std::uint32_t primitive_type,
			const int vertex_base,
			const int primitive_count);

		void draw(
			const std::uint32_t primitive_type,
			const int index_base,
			const int vertex_base,
			const int primitive_count);


	protected:
		VertexArrayObject();

		virtual ~VertexArrayObject();


	private:
		virtual bool do_initialize(
			const InitializeParam& param) = 0;

		virtual void do_uninitialize() = 0;

		virtual void do_set_vertex_data(
			const int vertex_index,
			const int vertex_count,
			const void* const raw_data) = 0;

		virtual void do_draw(
			const std::uint32_t primitive_type,
			const int index_base,
			const int vertex_base,
			const int primitive_count) = 0;
	}; // VertexArrayObject

	using VertexArrayObjectPtr = VertexArrayObject*;


	class Texture
	{
	public:
		static constexpr auto max_types = 2;


		enum class Type
		{
			none = 0,
			two_d = 1,
			cube_map = 2,
		}; // Type

		enum class UploadFilter
		{
			none,
			linear,
		}; // UploadFilter


		struct InitializeParam
		{
			Type type_;
			std::uint32_t surface_format_;
			int width_;
			int height_;
			int level_count_;


			InitializeParam();

			bool is_valid() const;
		}; // InitializeParam

		struct UploadParam
		{
			bool has_linear_filter_;
			int level_;
			int cube_face_index_;
			std::uint32_t src_surface_format_;
			int src_pitch_;
			const void* raw_data_;


			UploadParam();

			bool is_valid() const;
		}; // InitializeParam


		bool initialize(
			const InitializeParam& param);

		void uninitialize();


		bool upload_level(
			const UploadParam& param);


		Type get_type() const;

		bool is_2d() const;

		bool is_cube_map() const;

		std::uint32_t get_surface_format() const;

		bool is_compressed() const;

		int get_width() const;

		int get_height() const;

		int get_level_count() const;

		int get_level_width(
			const int level) const;

		int get_level_height(
			const int level) const;


	protected:
		Texture();

		virtual ~Texture();


	private:
		virtual bool do_initialize(
			const InitializeParam& param) = 0;

		virtual void do_uninitialize() = 0;


		virtual bool do_upload_level(
			const UploadParam& param) = 0;


		virtual Type do_get_type() const = 0;

		virtual std::uint32_t do_get_surface_format() const = 0;

		virtual bool do_is_compressed() const = 0;

		virtual int do_get_width() const = 0;

		virtual int do_get_height() const = 0;

		virtual int do_get_level_count() const = 0;

		virtual int do_get_level_width(
			const int level) const = 0;

		virtual int do_get_level_height(
			const int level) const = 0;
	}; // Texture

	using TexturePtr = Texture*;


	class Sampler
	{
	public:
		std::uint32_t get_addressing_mode_u() const;

		void set_addressing_mode_u(
			const std::uint32_t addressing_mode_u);


		std::uint32_t get_addressing_mode_v() const;

		void set_addressing_mode_v(
			const std::uint32_t addressing_mode_v);


		std::uint32_t get_mag_filter() const;

		void set_mag_filter(
			const std::uint32_t mag_filter);


		std::uint32_t get_min_filter() const;

		void set_min_filter(
			const std::uint32_t min_filter);


		std::uint32_t get_mip_filter() const;

		void set_mip_filter(
			const std::uint32_t mip_filter);


		float get_lod_bias() const;

		void set_lod_bias(
			const float mipmap_lod_bias);


		float get_anisotropy() const;

		void set_anisotropy(
			const float anisotropy);


	protected:
		Sampler();

		virtual ~Sampler();


	private:
		virtual std::uint32_t do_get_addressing_mode_u() const = 0;

		virtual void do_set_addressing_mode_u(
			const std::uint32_t address_mode_u) = 0;


		virtual std::uint32_t do_get_addressing_mode_v() const = 0;

		virtual void do_set_addressing_mode_v(
			const std::uint32_t address_mode_v) = 0;


		virtual std::uint32_t do_get_mag_filter() const = 0;

		virtual void do_set_mag_filter(
			const std::uint32_t mag_filter) = 0;


		virtual std::uint32_t do_get_min_filter() const = 0;

		virtual void do_set_min_filter(
			const std::uint32_t min_filter) = 0;


		virtual std::uint32_t do_get_mip_filter() const = 0;

		virtual void do_set_mip_filter(
			const std::uint32_t mip_filter) = 0;


		virtual float do_get_lod_bias() const = 0;

		virtual void do_set_lod_bias(
			const float lod_bias) = 0;


		virtual float do_get_anisotropy() const = 0;

		virtual void do_set_anisotropy(
			const float anisotropy) = 0;
	}; // Sampler

	using SamplerPtr = Sampler*;


	class Stage
	{
	public:
		TexturePtr get_texture();

		void set_texture(
			TexturePtr texture);


		std::uint32_t get_color_op() const;

		void set_color_op(
			const std::uint32_t color_op);


		std::uint32_t get_color_arg1() const;

		void set_color_arg1(
			const std::uint32_t color_arg1);


		std::uint32_t get_color_arg2() const;

		void set_color_arg2(
			const std::uint32_t color_arg2);


		std::uint32_t get_alpha_op() const;

		void set_alpha_op(
			const std::uint32_t alpha_op);


		std::uint32_t get_alpha_arg1() const;

		void set_alpha_arg1(
			const std::uint32_t alpha_arg1);


		std::uint32_t get_alpha_arg2() const;

		void set_alpha_arg2(
			const std::uint32_t alpha_arg2);


		std::uint32_t get_coord_index() const;

		void set_coord_index(
			const std::uint32_t coord_index);


		std::uint32_t get_trans_flags() const;

		void set_trans_flags(
			const std::uint32_t trans_flags);


		float get_bump_map_lum_scale() const;

		void set_bump_map_lum_scale(
			const float lum_scale);


		float get_bump_map_lum_offset() const;

		void set_bump_map_lum_offset(
			const float lum_offset);


		float get_bump_map_coefficient(
			const int index) const;

		void set_bump_map_coefficient(
			const int index,
			const float coefficient);


	protected:
		Stage();

		virtual ~Stage();


	private:
		virtual TexturePtr do_get_texture() = 0;

		virtual void do_set_texture(
			TexturePtr texture) = 0;


		virtual std::uint32_t do_get_color_op() const = 0;

		virtual void do_set_color_op(
			const std::uint32_t color_operation) = 0;


		virtual std::uint32_t do_get_color_arg1() const = 0;

		virtual void do_set_color_arg1(
			const std::uint32_t color_argument1) = 0;


		virtual std::uint32_t do_get_color_arg2() const = 0;

		virtual void do_set_color_arg2(
			const std::uint32_t color_argument2) = 0;


		virtual std::uint32_t do_get_alpha_op() const = 0;

		virtual void do_set_alpha_op(
			const std::uint32_t alpha_operation) = 0;


		virtual std::uint32_t do_get_alpha_arg1() const = 0;

		virtual void do_set_alpha_arg1(
			const std::uint32_t alpha_argument1) = 0;


		virtual std::uint32_t do_get_alpha_arg2() const = 0;

		virtual void do_set_alpha_arg2(
			const std::uint32_t alpha_argument2) = 0;


		virtual std::uint32_t do_get_coord_index() const = 0;

		virtual void do_set_coord_index(
			const std::uint32_t coord_index) = 0;


		virtual std::uint32_t do_get_trans_flags() const = 0;

		virtual void do_set_trans_flags(
			const std::uint32_t transformation_flags) = 0;


		virtual float do_get_bump_map_lum_scale() const = 0;

		virtual void do_set_bump_map_lum_scale(
			const float luminance_scale) = 0;


		virtual float do_get_bump_map_lum_offset() const = 0;

		virtual void do_set_bump_map_lum_offset(
			const float luminance_offset) = 0;


		virtual float do_get_bump_map_coefficient(
			const int index) const = 0;

		virtual void do_set_bump_map_coefficient(
			const int index,
			const float coefficient) = 0;
	}; // Stage

	using StagePtr = Stage*;


	bool is_initialized() const;

	bool initialize(
		const int screen_width,
		const int screen_height);

	void uninitialize();


#if 0
	bool get_is_current_context() const;

	void set_is_current_context(
		const bool is_current_context);

	bool set_post_is_current_context(
		const bool is_current_context);
#endif

	void set_clear_color(
		const std::uint8_t r,
		const std::uint8_t g,
		const std::uint8_t b,
		const std::uint8_t a = 0xFF);

	void clear(
		const std::uint32_t clear_flags);

	//
	// Notes:
	//    - Uses Direct3D coordinate system.
	//
	const Viewport& get_viewport() const;

	//
	// Notes:
	//    - Uses Direct3D coordinate system.
	//
	void set_viewport(
		const Viewport& viewport);


	std::uint32_t get_cull_mode() const;

	void set_cull_mode(
		const std::uint32_t cull_mode);


	std::uint32_t get_fill_mode() const;

	void set_fill_mode(
		const std::uint32_t fill_mode);


	bool get_is_clipping() const;

	void set_is_clipping(
		const bool is_clipping);


	bool is_depth_enabled() const;

	void set_is_depth_enabled(
		const bool is_enabled);


	bool is_depth_writable() const;

	void set_is_depth_writable(
		const bool is_writable);


	std::uint32_t get_depth_func() const;

	void set_depth_func(
		const std::uint32_t depth_func);


	bool get_is_blending_enabled() const;

	void set_is_blending_enabled(
		const bool is_blending_enabled);


	std::uint32_t get_src_blending_factor() const;

	std::uint32_t get_dst_blending_factor() const;

	void set_blending_factors(
		const std::uint32_t src_factor,
		const std::uint32_t dst_factor);


	const float* get_world_matrix(
		const int index) const;

	void set_world_matrix(
		const int index,
		const float* const world_matrix_ptr);


	const float* get_view_matrix() const;

	void set_view_matrix(
		const float* const view_matrix_ptr);


	const float* get_projection_matrix() const;

	void set_projection_matrix(
		const float* const projection_matrix_ptr);


	SamplerPtr get_sampler(
		const int index);


	VertexArrayObjectPtr add_vertex_array_object();

	void remove_vertex_array_object(
		VertexArrayObjectPtr vertex_array_object);


	TexturePtr add_texture();

	void remove_texture(
		TexturePtr texture);


	void draw(
		const std::uint32_t primitive_type,
		const std::uint32_t d3d_fvf,
		const int primitive_count,
		const void* const raw_data);


	static void ogl_clear_error();

	static bool ogl_is_succeed();


	static OglRenderer& get_instance();


protected:
	OglRenderer();

	virtual ~OglRenderer();


private:
	OglRenderer(
		const OglRenderer& that) = delete;

	OglRenderer& operator=(
		const OglRenderer& that) = delete;


	virtual bool do_is_initialized() const = 0;

	virtual bool do_initialize(
		const int screen_width,
		const int screen_height) = 0;

	virtual void do_uninitialize() = 0;

#if 0
	virtual bool do_get_is_current_context() const = 0;

	virtual void do_set_is_current_context(
		const bool is_current) = 0;
#endif

	// Clearing.
	//

	virtual void do_set_clear_color(
		const std::uint8_t r,
		const std::uint8_t g,
		const std::uint8_t b,
		const std::uint8_t a) = 0;

	virtual void do_clear(
		const std::uint32_t clear_flags) = 0;

	// Viewport.
	//

	virtual const Viewport& do_get_viewport() const = 0;

	virtual void do_set_viewport(
		const Viewport& viewport) = 0;

	// Culling.
	//

	virtual std::uint32_t do_get_cull_mode() const = 0;

	virtual void do_set_cull_mode(
		const std::uint32_t cull_mode) = 0;

	// Fill mode.
	//

	virtual std::uint32_t do_get_fill_mode() const = 0;

	virtual void do_set_fill_mode(
		const std::uint32_t fill_mode) = 0;

	// Clipping.
	//

	virtual bool do_get_is_clipping() const = 0;

	virtual void do_set_is_clipping(
		const bool is_clipping) = 0;

	// Depth buffer.
	//

	virtual bool do_is_depth_enabled() const = 0;

	virtual void do_set_is_depth_enabled(
		const bool is_enabled) = 0;

	virtual bool do_is_depth_writable() const = 0;

	virtual void do_set_is_depth_writable(
		const bool is_writable) = 0;

	virtual std::uint32_t do_get_depth_func() const = 0;

	virtual void do_set_depth_func(
		const std::uint32_t depth_func) = 0;

	// Alpha blending.
	//

	virtual bool do_get_is_blending_enabled() const = 0;

	virtual void do_set_is_blending_enabled(
		const bool is_blending_enabled) = 0;


	virtual std::uint32_t do_get_src_blending_factor() const = 0;

	virtual std::uint32_t do_get_dst_blending_factor() const = 0;

	virtual void do_set_blending_factors(
		const std::uint32_t src_factor,
		const std::uint32_t dst_factor) = 0;

	// Transformation matrices.
	//

	virtual const float* do_get_world_matrix(
		const int index) const = 0;

	virtual void do_set_world_matrix(
		const int index,
		const float* const world_matrix_ptr) = 0;


	virtual const float* do_get_view_matrix() const = 0;

	virtual void do_set_view_matrix(
		const float* const view_matrix_ptr) = 0;


	virtual const float* do_get_projection_matrix() const = 0;

	virtual void do_set_projection_matrix(
		const float* const projection_matrix_ptr) = 0;


	// Sampler.
	//

	virtual SamplerPtr do_get_sampler(
		const int index) = 0;

	// Vertex array objects.
	//
	virtual VertexArrayObjectPtr do_add_vertex_array_object() = 0;

	virtual void do_remove_vertex_array_object(
		VertexArrayObjectPtr vertex_array_object) = 0;

	// Textures.
	//
	virtual TexturePtr do_add_texture() = 0;

	virtual void do_remove_texture(
		TexturePtr texture) = 0;


	// Primitive drawing.
	//
	virtual void do_draw(
		const std::uint32_t primitive_type,
		const std::uint32_t d3d_fvf,
		const int primitive_count,
		const void* const raw_data) = 0;
}; // OglRenderer


} // ltjs


#endif // LTJS_OGL_RENDERER_INCLUDED
