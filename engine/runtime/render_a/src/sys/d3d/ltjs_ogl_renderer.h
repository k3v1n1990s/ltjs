#ifndef LTJS_OGL_RENDERER_INCLUDED
#define LTJS_OGL_RENDERER_INCLUDED


#include <cstdint>


namespace ltjs
{


class OglRenderer
{
public:
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

	virtual const Viewport& do_get_viewport() const = 0;

	virtual void do_set_viewport(
		const Viewport& viewport) = 0;
}; // OglRenderer


} // ltjs


#endif // LTJS_OGL_RENDERER_INCLUDED
