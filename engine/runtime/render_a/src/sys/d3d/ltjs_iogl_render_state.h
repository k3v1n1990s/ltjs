#ifndef LTJS_IOGL_RENDER_STATE_INCLUDED
#define LTJS_IOGL_RENDER_STATE_INCLUDED


#include <cstdint>


namespace ltjs
{


class IOglRenderState
{
public:
	bool is_initialized() const;

	bool initialize(
		const int screen_width,
		const int screen_height);

	void uninitialize();


	void set_clear_color(
		const std::uint8_t r,
		const std::uint8_t g,
		const std::uint8_t b,
		const std::uint8_t a = 0xFF);

	//
	// Notes:
	//    - Uses Direct3D coordinate system.
	//
	void set_viewport(
		const int x,
		const int y,
		const int width,
		const int height);

	void set_depth_range(
		const float min_z,
		const float max_z);

	void ogl_clear_error();

	bool ogl_is_succeed();


	static IOglRenderState* get_instance();


protected:
	IOglRenderState();

	virtual ~IOglRenderState();


private:
	IOglRenderState(
		const IOglRenderState& that) = delete;

	IOglRenderState& operator=(
		const IOglRenderState& that) = delete;


	virtual bool do_is_initialized() const = 0;

	virtual bool do_initialize(
		const int screen_width,
		const int screen_height) = 0;

	virtual void do_uninitialize() = 0;

	virtual void do_set_clear_color(
		const std::uint8_t r,
		const std::uint8_t g,
		const std::uint8_t b,
		const std::uint8_t a) = 0;

	virtual void do_set_viewport(
		const int x,
		const int y,
		const int width,
		const int height) = 0;

	virtual void do_set_depth_range(
		const float min_z,
		const float max_z) = 0;
}; // IOglRenderState


} // ltjs


#endif // LTJS_IOGL_RENDER_STATE_INCLUDED
