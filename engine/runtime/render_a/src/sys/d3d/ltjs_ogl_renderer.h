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
	static constexpr auto max_samplers = 4;


	struct ClearFlags :
		public ul::EnumFlags
	{
		ClearFlags(
			const Value flags = none)
			:
			ul::EnumFlags{flags}
		{
		}

		enum : Value
		{
			target = 1,
			zbuffer = 2,
			stencil = 4,
		}; // enum
	}; // ClearFlags

	enum class CullMode
	{
		none = 1,
		cw = 2,
		ccw = 3,
	}; // CullMode

	enum class FillMode :
		std::uint32_t
	{
		none = 0,
		wireframe = 2,
		solid = 3,
	}; // FillMode

	enum class CompareFunc :
		std::uint32_t
	{
		none = 0,
		always = 8,
		equal = 3,
		greater = 5,
		greater_or_equal = 7,
		less = 2,
		less_or_equal = 4,
		not_equal = 6,
	}; // CompareFunc

	enum class BlendingFactor :
		std::uint32_t
	{
		none = 0,
		zero = 1,
		one = 2,
		src_alpha = 5,
		src_color = 3,
		inv_src_alpha = 6,
		inv_src_color = 4,
		dst_color = 9,
		inv_dst_color = 10,
	}; // BlendingFactor

	enum class PrimitiveType
	{
		none = 0,
		triangle_strip = 5,
		triangle_fan = 6,
		triangle_list = 4,
	}; // PrimitiveType

	enum class TextureAddressingMode :
		std::uint32_t
	{
		none = 0,
		clamp = 3,
		wrap = 1,
	}; // TextureAddresingsMode

	enum class TextureFilterType :
		std::uint32_t
	{
		none = 0,
		point = 1,
		linear = 2,
		anisotropic = 3,
	}; // TextureFilterType

	enum class TextureOp :
		std::uint32_t
	{
		none = 0,
		disable = 1,
		select_arg1 = 2,
		select_arg2 = 3,
		modulate = 4,
		modulate_2x = 5,
		add = 7,
		add_signed = 8,
		subtract = 10,
		blend_current_alpha = 16,
		modulate_alpha_add_color = 18,
		bump_envmap = 22,
		bump_envmap_luminance = 23,
		dot_product3 = 24,
	}; // TextureOp

	struct TextureArg :
		public ul::EnumFlags
	{
		TextureArg(
			const Value flags = none)
			:
			ul::EnumFlags{flags}
		{
		}

		enum : Value
		{
			diffuse = 0x00000000,
			current = 0x00000001,
			texture = 0x00000002,
			factor = 0x00000003,
			complement = 0x00000010,
		}; // enum
	}; // TextureArg

	struct TextureTransFlags :
		public ul::EnumFlags
	{
		TextureTransFlags(
			const Value flags = none)
			:
			ul::EnumFlags{flags}
		{
		}

		enum : Value
		{
			disable = 0,
			count2 = 2,
			count3 = 3,
			projected = 256,
		}; // enum
	}; // TextureTransFlags

	struct TextureCoordIndex :
		public ul::EnumFlags
	{
		TextureCoordIndex(
			const Value flags = none)
			:
			ul::EnumFlags{flags}
		{
		}

		enum : Value
		{
			pass_through = 0x00000000,
			camera_space_position = 0x00020000,
			camera_space_reflection_vector = 0x00030000,
		}; // enum
	}; // TextureCoordIndex

	enum class SurfaceFormat :
		std::uint32_t
	{
		none = 0,
		a4r4g4b4 = 26,
		a8r8g8b8 = 21,
		v8u8 = 60,
		dxt1 = ul::FourCc{"DXT1"},
		dxt3 = ul::FourCc{"DXT3"},
		dxt5 = ul::FourCc{"DXT5"},
	}; // SurfaceFormat


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

	// Flexible vertex format.
	struct Fvf
	{
		using TexCoordItemCounts = std::array<int, max_samplers>;


		// Has position? (D3DFVF_XYZ or D3DFVF_XYZRHW)
		bool has_position_;

		// Is position already transformed? (D3DFVF_XYZRHW)
		bool is_position_transformed_;


		// Geometry blending weight count.
		int blending_weight_count_;

		// Has normals? (D3DFVF_NORMAL)
		bool has_normal_;

		// Has diffuse color? (D3DFVF_DIFFUSE)
		bool has_diffuse_;

		// Texture coordinate set count.
		int tex_coord_set_count_;

		// Component count per texture coordinate set.
		TexCoordItemCounts tex_coord_item_counts_;

		// Total vertex size.
		int vertex_size_;


		Fvf();

		Fvf(
			const std::uint32_t d3d_fvf);

		// Has geometry blending weights? (D3DFVF_XYZB#)
		bool has_blending_weights() const;

		// Has texture coordinate set? (D3DFVF_TEX#)
		bool has_tex_coord_sets() const;


		bool is_valid() const;


		static Fvf from_d3d(
			const std::uint32_t d3d_fvf);
	}; // Fvf

	class VertexArrayObject
	{
	public:
		struct UsageFlags :
			public ul::EnumFlags
		{
			UsageFlags(
				const Value flags = none)
				:
				ul::EnumFlags{flags}
			{
			}

			enum : Value
			{
				is_dynamic = 1 << 0,
				is_readable = 1 << 1,
			}; // enum
		}; // VertexFormatFlags

		struct InitializeParam
		{
			bool has_index_;
			Fvf vertex_format_;
			UsageFlags vertex_usage_flags_;
			int vertex_count_;
			const void* raw_vertex_data_;
			UsageFlags index_usage_flags_;
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
			const PrimitiveType primitive_type,
			const int primitive_count);

		void draw(
			const PrimitiveType primitive_type,
			const int vertex_base,
			const int primitive_count);

		void draw(
			const PrimitiveType primitive_type,
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
			const PrimitiveType primitive_type,
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
			SurfaceFormat surface_format_;
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
			SurfaceFormat src_surface_format_;
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

		SurfaceFormat get_surface_format() const;

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

		virtual SurfaceFormat do_get_surface_format() const = 0;

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
		TextureAddressingMode get_addressing_mode_u() const;

		void set_addressing_mode_u(
			const TextureAddressingMode addressing_mode_u);


		TextureAddressingMode get_addressing_mode_v() const;

		void set_addressing_mode_v(
			const TextureAddressingMode addressing_mode_v);


		TextureFilterType get_mag_filter() const;

		void set_mag_filter(
			const TextureFilterType mag_filter);


		TextureFilterType get_min_filter() const;

		void set_min_filter(
			const TextureFilterType min_filter);


		TextureFilterType get_mip_filter() const;

		void set_mip_filter(
			const TextureFilterType mip_filter);


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
		virtual TextureAddressingMode do_get_addressing_mode_u() const = 0;

		virtual void do_set_addressing_mode_u(
			const TextureAddressingMode address_mode_u) = 0;


		virtual TextureAddressingMode do_get_addressing_mode_v() const = 0;

		virtual void do_set_addressing_mode_v(
			const TextureAddressingMode address_mode_v) = 0;


		virtual TextureFilterType do_get_mag_filter() const = 0;

		virtual void do_set_mag_filter(
			const TextureFilterType mag_filter) = 0;


		virtual TextureFilterType do_get_min_filter() const = 0;

		virtual void do_set_min_filter(
			const TextureFilterType min_filter) = 0;


		virtual TextureFilterType do_get_mip_filter() const = 0;

		virtual void do_set_mip_filter(
			const TextureFilterType mip_filter) = 0;


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


		TextureOp get_color_op() const;

		void set_color_op(
			const TextureOp& color_op);


		TextureArg get_color_arg1() const;

		void set_color_arg1(
			const TextureArg& color_arg1);


		TextureArg get_color_arg2() const;

		void set_color_arg2(
			const TextureArg& color_arg2);


		TextureOp get_alpha_op() const;

		void set_alpha_op(
			const TextureOp& alpha_op);


		TextureArg get_alpha_arg1() const;

		void set_alpha_arg1(
			const TextureArg& alpha_arg1);


		TextureArg get_alpha_arg2() const;

		void set_alpha_arg2(
			const TextureArg& alpha_arg2);


		TextureCoordIndex get_coord_index() const;

		void set_coord_index(
			const TextureCoordIndex& coord_index);


		TextureTransFlags get_trans_flags() const;

		void set_trans_flags(
			const TextureTransFlags& trans_flags);


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


		virtual TextureOp do_get_color_op() const = 0;

		virtual void do_set_color_op(
			const TextureOp& color_operation) = 0;


		virtual TextureArg do_get_color_arg1() const = 0;

		virtual void do_set_color_arg1(
			const TextureArg& color_argument1) = 0;


		virtual TextureArg do_get_color_arg2() const = 0;

		virtual void do_set_color_arg2(
			const TextureArg& color_argument2) = 0;


		virtual TextureOp do_get_alpha_op() const = 0;

		virtual void do_set_alpha_op(
			const TextureOp& alpha_operation) = 0;


		virtual TextureArg do_get_alpha_arg1() const = 0;

		virtual void do_set_alpha_arg1(
			const TextureArg& alpha_argument1) = 0;


		virtual TextureArg do_get_alpha_arg2() const = 0;

		virtual void do_set_alpha_arg2(
			const TextureArg& alpha_argument2) = 0;


		virtual TextureCoordIndex do_get_coord_index() const = 0;

		virtual void do_set_coord_index(
			const TextureCoordIndex& coord_index) = 0;


		virtual TextureTransFlags do_get_trans_flags() const = 0;

		virtual void do_set_trans_flags(
			const TextureTransFlags& transformation_flags) = 0;


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


	bool get_is_current_context() const;

	void set_is_current_context(
		const bool is_current_context);

	bool set_post_is_current_context(
		const bool is_current_context);

	void set_clear_color(
		const std::uint8_t r,
		const std::uint8_t g,
		const std::uint8_t b,
		const std::uint8_t a = 0xFF);

	void clear(
		const ClearFlags clear_flags);

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


	CullMode get_cull_mode() const;

	void set_cull_mode(
		const CullMode cull_mode);


	FillMode get_fill_mode() const;

	void set_fill_mode(
		const FillMode fill_mode);


	bool get_is_clipping() const;

	void set_is_clipping(
		const bool is_clipping);


	bool is_depth_enabled() const;

	void set_is_depth_enabled(
		const bool is_enabled);


	bool is_depth_writable() const;

	void set_is_depth_writable(
		const bool is_writable);


	CompareFunc get_depth_func() const;

	void set_depth_func(
		const CompareFunc depth_func);


	bool get_is_blending_enabled() const;

	void set_is_blending_enabled(
		const bool is_blending_enabled);


	BlendingFactor get_src_blending_factor() const;

	BlendingFactor get_dst_blending_factor() const;

	void set_blending_factors(
		const BlendingFactor src_factor,
		const BlendingFactor dst_factor);


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
		const PrimitiveType primitive_type,
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

	virtual bool do_get_is_current_context() const = 0;

	virtual void do_set_is_current_context(
		const bool is_current) = 0;

	// Clearing.
	//

	virtual void do_set_clear_color(
		const std::uint8_t r,
		const std::uint8_t g,
		const std::uint8_t b,
		const std::uint8_t a) = 0;

	virtual void do_clear(
		const ClearFlags clear_flags) = 0;

	// Viewport.
	//

	virtual const Viewport& do_get_viewport() const = 0;

	virtual void do_set_viewport(
		const Viewport& viewport) = 0;

	// Culling.
	//

	virtual CullMode do_get_cull_mode() const = 0;

	virtual void do_set_cull_mode(
		const CullMode cull_mode) = 0;

	// Fill mode.
	//

	virtual FillMode do_get_fill_mode() const = 0;

	virtual void do_set_fill_mode(
		const FillMode fill_mode) = 0;

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

	virtual CompareFunc do_get_depth_func() const = 0;

	virtual void do_set_depth_func(
		const CompareFunc depth_func) = 0;

	// Alpha blending.
	//

	virtual bool do_get_is_blending_enabled() const = 0;

	virtual void do_set_is_blending_enabled(
		const bool is_blending_enabled) = 0;


	virtual BlendingFactor do_get_src_blending_factor() const = 0;

	virtual BlendingFactor do_get_dst_blending_factor() const = 0;

	virtual void do_set_blending_factors(
		const BlendingFactor src_factor,
		const BlendingFactor dst_factor) = 0;

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
		const PrimitiveType primitive_type,
		const std::uint32_t d3d_fvf,
		const int primitive_count,
		const void* const raw_data) = 0;
}; // OglRenderer


} // ltjs


#endif // LTJS_OGL_RENDERER_INCLUDED
