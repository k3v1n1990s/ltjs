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

		// Flexible vertex format.
		struct Fvf
		{
			static constexpr auto max_tex_coord_sets = 4;

			using TexCoordItemCounts = std::array<int, max_tex_coord_sets>;


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
			bool has_tex_coord_sets() const;;


			static Fvf from_d3d(
				const std::uint32_t d3d_fvf);
		}; // Fvf


	protected:
		VertexArrayObject();

		virtual ~VertexArrayObject();


	private:
	}; // VertexArrayObject


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


	void ogl_clear_error();

	bool ogl_is_succeed();


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
}; // OglRenderer


} // ltjs


#endif // LTJS_OGL_RENDERER_INCLUDED
