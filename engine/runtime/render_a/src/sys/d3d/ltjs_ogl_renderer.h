#ifndef LTJS_OGL_RENDERER_INCLUDED
#define LTJS_OGL_RENDERER_INCLUDED


#include <cstdint>
#include <array>
#include "bibendovsky_spul_enum_flags.h"


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
			color = 1 << 0,
			depth = 1 << 1,
			stencil = 1 << 2,
		}; // enum
	}; // ClearFlags

	enum class CullMode
	{
		none,
		disabled,
		clockwise,
		counterclockwise,
	}; // CullMode

	enum class FillMode
	{
		none,
		wireframe,
		solid,
	}; // FillMode

	enum class DepthFunc
	{
		none,
		always,
		equal,
		greater,
		greater_or_equal,
		less,
		lees_or_equal,
		not_equal,
	}; // DepthFunc

	enum class PrimitiveType
	{
		none,
		triangle_strip,
		triangle_fan,
		triangle_list,
	}; // PrimitiveType


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


	class SamplerState
	{
	public:
		enum class AddressMode
		{
			none,
			clamp,
			wrap,
		}; // AddressMode

		enum class Filter
		{
			none,
			disabled,
			point,
			linear,
			anisotropic,
		}; // Filter


		AddressMode get_address_mode_u() const;

		void set_address_mode_u(
			const AddressMode address_mode_u);


		AddressMode get_address_mode_v() const;

		void set_address_mode_v(
			const AddressMode address_mode_v);


		Filter get_mag_filter() const;

		void set_mag_filter(
			const Filter mag_filter);


		Filter get_min_filter() const;

		void set_min_filter(
			const Filter min_filter);


		Filter get_mip_filter() const;

		void set_mip_filter(
			const Filter mip_filter);


		float get_mipmap_lod_bias() const;

		void set_mipmap_lod_bias(
			const float mipmap_lod_bias);


		int get_max_anisotropy() const;

		void set_max_anisotropy(
			const int max_anisotropy);


	protected:
		SamplerState();

		virtual ~SamplerState();


	private:
		virtual AddressMode do_get_address_mode_u() const = 0;

		virtual void do_set_address_mode_u(
			const AddressMode address_mode_u) = 0;


		virtual AddressMode do_get_address_mode_v() const = 0;

		virtual void do_set_address_mode_v(
			const AddressMode address_mode_v) = 0;


		virtual Filter do_get_mag_filter() const = 0;

		virtual void do_set_mag_filter(
			const Filter mag_filter) = 0;


		virtual Filter do_get_min_filter() const = 0;

		virtual void do_set_min_filter(
			const Filter min_filter) = 0;


		virtual Filter do_get_mip_filter() const = 0;

		virtual void do_set_mip_filter(
			const Filter mip_filter) = 0;


		virtual float do_get_mipmap_lod_bias() const = 0;

		virtual void do_set_mipmap_lod_bias(
			const float mipmap_lod_bias) = 0;


		virtual int do_get_max_anisotropy() const = 0;

		virtual void do_set_max_anisotropy(
			const int max_anisotropy) = 0;
	}; // SamplerState

	using SamplerStatePtr = SamplerState*;



	bool is_initialized() const;

	bool initialize(
		const int screen_width,
		const int screen_height);

	void uninitialize();


	void set_current_context(
		const bool is_current);

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


	DepthFunc get_depth_func() const;

	void set_depth_func(
		const DepthFunc depth_func);


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


	SamplerStatePtr get_sampler_state(
		const int index);


	VertexArrayObjectPtr add_vertex_array_object();

	void remove_vertex_array_object(
		VertexArrayObjectPtr vertex_array_object);


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

	virtual void do_set_current_context(
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

	virtual DepthFunc do_get_depth_func() const = 0;

	virtual void do_set_depth_func(
		const DepthFunc depth_func) = 0;

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


	// Sampler state.
	//

	virtual SamplerStatePtr do_get_sampler_state(
		const int index) = 0;

	// Vertex array objects.
	//
	virtual VertexArrayObjectPtr do_add_vertex_array_object() = 0;

	virtual void do_remove_vertex_array_object(
		VertexArrayObjectPtr vertex_array_object) = 0;


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
