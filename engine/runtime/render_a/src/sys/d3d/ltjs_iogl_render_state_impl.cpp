#include "precompile.h"
#include "ltjs_iogl_render_state.h"
#include <cassert>
#include <array>
#include <vector>
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
		is_context_current_{},
		ogl_dc_{},
		ogl_rc_{},
		screen_width_{},
		screen_height_{},
		clear_color_r_{},
		clear_color_g_{},
		clear_color_b_{},
		clear_color_a_{},
		viewport_{},
		max_viewport_size_{}
	{
	}

	~IOglRenderStateImpl() override
	{
		do_uninitialize_internal();
	}


private:
	using ViewportSize = std::array<int, 2>;


	bool is_initialized_;
	bool is_context_current_;

	HDC ogl_dc_;
	HGLRC ogl_rc_;

	int screen_width_;
	int screen_height_;

	std::uint8_t clear_color_r_;
	std::uint8_t clear_color_g_;
	std::uint8_t clear_color_b_;
	std::uint8_t clear_color_a_;

	Viewport viewport_;
	ViewportSize max_viewport_size_;


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

	void do_set_current_context(
		const bool is_current)
	{
		if (!is_initialized_)
		{
			return;
		}

		if (is_current == is_context_current_)
		{
			return;
		}

		const auto dc = (is_current ? ogl_dc_ : nullptr);
		const auto rc = (is_current ? ogl_rc_ : nullptr);

		const auto make_result = ::wglMakeCurrent(dc, rc);
		assert(make_result);

		is_context_current_ = is_current;
	}

	void do_set_clear_color(
		const std::uint8_t r,
		const std::uint8_t g,
		const std::uint8_t b,
		const std::uint8_t a) override
	{
		assert(is_context_current_);

		if (!is_initialized_ || !is_context_current_)
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

	const Viewport& do_get_viewport() const override
	{
		return viewport_;
	}

	void do_set_viewport(
		const Viewport& viewport) override
	{
		assert(is_context_current_);

		if (!is_initialized_ || !is_context_current_)
		{
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

		if (viewport.x_ == viewport_.x_ &&
			viewport.y_ == viewport_.y_ &&
			viewport.width_ == viewport_.width_ &&
			viewport.height_ == viewport_.height_ &&
			viewport.depth_min_z_ == viewport_.depth_min_z_ &&
			viewport.depth_max_z_ == viewport_.depth_max_z_)
		{
			return;
		}

		viewport_ = viewport;

		do_set_viewport_internal();
	}

	void do_set_clear_color_internal()
	{
		::glClearColor(
			clear_color_r_ / 255.0F,
			clear_color_g_ / 255.0F,
			clear_color_b_ / 255.0F,
			clear_color_a_ / 255.0F);
	}

	void do_set_viewport_internal()
	{
		const auto ogl_viewport_y = screen_height_ - (viewport_.y_ + viewport_.height_);

		::glViewport(viewport_.x_, ogl_viewport_y, viewport_.width_, viewport_.height_);
		::glDepthRange(viewport_.depth_min_z_, viewport_.depth_max_z_);
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

		ogl_dc_ = ::wglGetCurrentDC();
		ogl_rc_ = ::wglGetCurrentContext();

		if (!ogl_dc_ || !ogl_rc_)
		{
			return false;
		}

		// Implementation defined values.
		//
		::glGetIntegerv(GL_MAX_VIEWPORT_DIMS, max_viewport_size_.data());

		if (!ogl_is_succeed())
		{
			return false;
		}

		if (max_viewport_size_[0] <= 0 ||
			max_viewport_size_[1] <= 0 ||
			screen_width > max_viewport_size_[0] ||
			screen_height > max_viewport_size_[1])
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

		viewport_.x_ = 0;
		viewport_.y_ = 0;
		viewport_.width_ = screen_width_;
		viewport_.height_ = screen_height_;
		viewport_.depth_min_z_ = 0.0F;
		viewport_.depth_max_z_ = 1.0F;
		do_set_viewport_internal();

		if (!ogl_is_succeed())
		{
			return false;
		}

		is_initialized_ = true;
		is_context_current_ = true;

		return true;
	}

	void do_uninitialize_internal()
	{
		is_initialized_ = false;
		is_context_current_ = false;

		ogl_dc_ = nullptr;
		ogl_rc_ = nullptr;

		screen_width_ = 0;
		screen_height_ = 0;

		clear_color_r_ = 0;
		clear_color_g_ = 0;
		clear_color_b_ = 0;
		clear_color_a_ = 0;

		viewport_ = {};
		max_viewport_size_ = {};
	}
}; // IOglRenderStateImpl


IOglRenderState::Viewport::Viewport()
	:
	x_{},
	y_{},
	width_{},
	height_{},
	depth_min_z_{},
	depth_max_z_{}
{
}


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

void IOglRenderState::set_current_context(
	const bool is_current)
{
	do_set_current_context(is_current);
}

void IOglRenderState::set_clear_color(
	const std::uint8_t r,
	const std::uint8_t g,
	const std::uint8_t b,
	const std::uint8_t a)
{
	do_set_clear_color(r, g, b, a);
}

const IOglRenderState::Viewport& IOglRenderState::get_viewport() const
{
	return do_get_viewport();
}

void IOglRenderState::set_viewport(
	const Viewport& viewport)
{
	do_set_viewport(viewport);
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

