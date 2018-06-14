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
		max_viewport_size_{},
		state_stack_{}
	{
	}

	~IOglRenderStateImpl() override
	{
		do_uninitialize_internal();
	}


private:
	static constexpr auto state_stack_reserve_size = 4;


	struct StateStackItem
	{
		static constexpr auto max_values = 6;

		struct Value
		{
			union
			{
				int as_int_;
				float as_float_;
			}; // union
		}; // Value

		using Values = std::array<Value, max_values>;


		StackStateItemType type_;
		Values values_;


		StateStackItem()
			:
			type_{},
			values_{}
		{
		}
	}; // StateStackItem

	using StateStack = std::vector<StateStackItem>;

	using ViewportSize = std::array<int, 2>;


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

	ViewportSize max_viewport_size_;

	StateStack state_stack_;


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
			x >= max_viewport_size_[0] ||
			y >= max_viewport_size_[1] ||
			width >= max_viewport_size_[0] ||
			height >= max_viewport_size_[1])
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

	void do_push_state_item(
		const StackStateItemType state_item_type) override
	{
		if (!is_initialized_)
		{
			return;
		}

		switch (state_item_type)
		{
		case StackStateItemType::viewport:
			do_push_state_viewport_internal();
			break;

		default:
			assert(!"Invalid item type.");
			throw "Invalid item type.";
		}
	}

	void do_pop_state_item() override
	{
		if (!is_initialized_)
		{
			return;
		}

		if (state_stack_.empty())
		{
			assert(!"Empty stack.");
			throw "Empty stack.";
		}

		do_pop_state_item_internal();
	}

	void do_pop_state_items() override
	{
		if (!is_initialized_)
		{
			return;
		}

		if (state_stack_.empty())
		{
			assert(!"Empty stack.");
			throw "Empty stack.";
		}

		while (!state_stack_.empty())
		{
			do_pop_state_item_internal();
		}
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

		state_stack_.reserve(state_stack_reserve_size);

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

		max_viewport_size_ = {};

		state_stack_.clear();
	}

	void do_push_state_viewport_internal()
	{
		state_stack_.emplace_back();
		auto& item = state_stack_.back();

		item.type_ = StackStateItemType::viewport;
		item.values_[0].as_int_ = viewport_x_;
		item.values_[1].as_int_ = viewport_y_;
		item.values_[2].as_int_ = viewport_width_;
		item.values_[3].as_int_ = viewport_height_;
		item.values_[4].as_float_ = viewport_depth_min_z_;
		item.values_[5].as_float_ = viewport_depth_max_z_;
	}

	void do_pop_state_viewport_internal()
	{
		const auto& item = state_stack_.back();

		do_set_viewport(
			item.values_[0].as_int_,
			item.values_[1].as_int_,
			item.values_[2].as_int_,
			item.values_[3].as_int_,
			item.values_[4].as_float_,
			item.values_[5].as_float_);

		state_stack_.pop_back();
	}

	void do_pop_state_item_internal()
	{
		const auto& item = state_stack_.back();

		switch (item.type_)
		{
		case StackStateItemType::viewport:
			do_pop_state_viewport_internal();
			break;

		default:
			assert(!"Invalid item type.");
			throw "Invalid item type.";
			break;
		}
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

void IOglRenderState::push_state_item(
	const StackStateItemType state_item_type)
{
	do_push_state_item(state_item_type);
}

void IOglRenderState::pop_state_item()
{
	do_pop_state_item();
}

void IOglRenderState::pop_state_items()
{
	do_pop_state_items();
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

