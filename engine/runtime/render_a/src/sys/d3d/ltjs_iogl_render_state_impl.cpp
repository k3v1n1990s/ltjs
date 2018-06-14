#include "precompile.h"
#include "ltjs_iogl_render_state.h"
#include "glad.h"


namespace ltjs
{


class IOglRenderStateImpl final :
	public IOglRenderState
{
public:
	IOglRenderStateImpl()
		:
		is_initialized_{},
		screen_width_{},
		screen_height_{},
		clear_color_r_{},
		clear_color_g_{},
		clear_color_b_{},
		clear_color_a_{},
		viewport_x_{},
		viewport_y_{},
		viewport_width_{},
		viewport_height_{},
		viewport_depth_min_z_{},
		viewport_depth_max_z_{},
		max_viewport_dims_{}
	{
	}

	~IOglRenderStateImpl() override
	{
		do_uninitialize_internal();
	}


private:
	bool is_initialized_;

	int screen_width_;
	int screen_height_;

	std::uint8_t clear_color_r_;
	std::uint8_t clear_color_g_;
	std::uint8_t clear_color_b_;
	std::uint8_t clear_color_a_;

	int viewport_x_;
	int viewport_y_;
	int viewport_width_;
	int viewport_height_;

	float viewport_depth_min_z_;
	float viewport_depth_max_z_;

	int max_viewport_dims_;


	bool do_is_initialized() const override
	{
		return is_initialized_;
	}

	bool do_initialize(
		const int screen_width,
		const int screen_height) override
	{
		if (!do_initialize_internal(screen_width, screen_height))
		{
			do_uninitialize_internal();
			return false;
		}

		return true;
	}

	void do_uninitialize() override
	{
		do_uninitialize_internal();
	}

	void do_set_clear_color(
		const std::uint8_t r,
		const std::uint8_t g,
		const std::uint8_t b,
		const std::uint8_t a) override
	{
		if (!is_initialized_)
		{
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

		do_set_clear_color_internal();
	}

	void do_set_clear_color_internal()
	{
		::glClearColor(
			clear_color_r_ / 255.0F,
			clear_color_g_ / 255.0F,
			clear_color_b_ / 255.0F,
			clear_color_a_ / 255.0F);
	}

	void do_set_viewport(
		const int x,
		const int y,
		const int width,
		const int height,
		const float min_z,
		const float max_z) override
	{
		if (!is_initialized_)
		{
			return;
		}

		if (x < 0 ||
			y < 0 ||
			width <= 0 ||
			height <= 0 ||
			x >= max_viewport_dims_ ||
			y >= max_viewport_dims_ ||
			width >= max_viewport_dims_ ||
			height >= max_viewport_dims_)
		{
			return;
		}

		if (x == viewport_x_ &&
			y == viewport_y_ &&
			width == viewport_width_ &&
			height == viewport_height_ &&
			min_z == viewport_depth_min_z_ &&
			max_z == viewport_depth_max_z_)
		{
			return;
		}

		viewport_x_ = x;
		viewport_y_ = y;
		viewport_width_ = width;
		viewport_height_ = height;

		viewport_depth_min_z_ = min_z;
		viewport_depth_max_z_ = max_z;

		do_set_viewport_internal();
	}

	void do_set_viewport_internal()
	{
		const auto ogl_viewport_y = screen_height_ - (viewport_y_ + viewport_height_);

		::glViewport(viewport_x_, ogl_viewport_y, viewport_width_, viewport_height_);
		::glDepthRange(viewport_depth_min_z_, viewport_depth_max_z_);
	}

	bool do_initialize_internal(
		const int screen_width,
		const int screen_height)
	{
		ogl_clear_error();

		if (screen_width <= 0 || screen_height <= 0)
		{
			return false;
		}

		// Implementation defined values.
		//
		::glGetIntegerv(GL_MAX_VIEWPORT_DIMS, &max_viewport_dims_);

		if (!ogl_is_succeed())
		{
			return false;
		}

		if (max_viewport_dims_ <= 0.0F ||
			screen_width > max_viewport_dims_ || screen_height > max_viewport_dims_)
		{
			return false;
		}

		// Set defaults.
		//
		screen_width_ = screen_width;
		screen_height_ = screen_height;

		clear_color_r_ = 0;
		clear_color_g_ = 0;
		clear_color_b_ = 0;
		clear_color_a_ = 0;
		do_set_clear_color_internal();

		viewport_x_ = 0;
		viewport_y_ = 0;
		viewport_width_ = screen_width_;
		viewport_height_ = screen_height_;
		viewport_depth_min_z_ = 0.0F;
		viewport_depth_max_z_ = 1.0F;
		do_set_viewport_internal();

		if (!ogl_is_succeed())
		{
			return false;
		}

		is_initialized_ = true;

		return true;
	}

	void do_uninitialize_internal()
	{
		is_initialized_ = false;

		screen_width_ = 0;
		screen_height_ = 0;

		clear_color_r_ = 0;
		clear_color_g_ = 0;
		clear_color_b_ = 0;
		clear_color_a_ = 0;

		viewport_x_ = 0;
		viewport_y_ = 0;
		viewport_width_ = 0;
		viewport_height_ = 0;

		viewport_depth_min_z_ = 0.0F;
		viewport_depth_max_z_ = 0.0F;

		max_viewport_dims_ = 0;
	}
}; // IOglRenderStateImpl


IOglRenderState::IOglRenderState()
{
}

IOglRenderState::~IOglRenderState()
{
}

bool IOglRenderState::is_initialized() const
{
	return do_is_initialized();
}

bool IOglRenderState::initialize(
	const int screen_width,
	const int screen_height)
{
	return do_initialize(screen_width, screen_height);
}

void IOglRenderState::uninitialize()
{
	do_uninitialize();
}

void IOglRenderState::set_clear_color(
	const std::uint8_t r,
	const std::uint8_t g,
	const std::uint8_t b,
	const std::uint8_t a)
{
	do_set_clear_color(r, g, b, a);
}

void IOglRenderState::set_viewport(
	const int x,
	const int y,
	const int width,
	const int height,
	const float min_z,
	const float max_z)
{
	do_set_viewport(x, y, width, height, min_z, max_z);
}

void IOglRenderState::ogl_clear_error()
{
	static_cast<void>(ogl_is_succeed());
}

bool IOglRenderState::ogl_is_succeed()
{
	auto is_failed = false;

	while (::glGetError() != GL_NO_ERROR)
	{
		is_failed = true;
	}

	return !is_failed;
}

IOglRenderState* IOglRenderState::get_instance()
{
	static IOglRenderStateImpl instance{};
	return &instance;
}


} // ltjs

