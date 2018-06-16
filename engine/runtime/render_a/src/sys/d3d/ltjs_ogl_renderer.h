#ifndef LTJS_OGL_RENDERER_INCLUDED
#define LTJS_OGL_RENDERER_INCLUDED


#include <cstdint>
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
		clockwise,
		counterclockwise,
	}; // CullMode


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

	virtual void do_set_clear_color(
		const std::uint8_t r,
		const std::uint8_t g,
		const std::uint8_t b,
		const std::uint8_t a) = 0;

	virtual void do_clear(
		const ClearFlags clear_flags) = 0;

	virtual const Viewport& do_get_viewport() const = 0;

	virtual void do_set_viewport(
		const Viewport& viewport) = 0;

	virtual CullMode do_get_cull_mode() const = 0;

	virtual void do_set_cull_mode(
		const CullMode cull_mode) = 0;

	virtual bool do_get_is_clipping() const = 0;

	virtual void do_set_is_clipping(
		const bool is_clipping) = 0;
}; // OglRenderer


} // ltjs


#endif // LTJS_OGL_RENDERER_INCLUDED