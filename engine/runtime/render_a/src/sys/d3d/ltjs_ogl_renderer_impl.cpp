#include "precompile.h"
#include "ltjs_ogl_renderer.h"
#include <cassert>
#include <array>
#include <vector>
#include "glad.h"


namespace ltjs
{


class OglRendererImpl final :
	public OglRenderer
{
public:
	OglRendererImpl()
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
		max_viewport_size_{},
		vertex_shader_{},
		fragment_shader_{},
		program_{}
	{
	}

	~OglRendererImpl() override
	{
		do_uninitialize_internal();
	}


private:
	using ViewportSize = std::array<int, 2>;

	enum class ShaderType
	{
		none,
		vertex,
		fragment,
	}; // ShaderType


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

	GLuint vertex_shader_;
	GLuint fragment_shader_;
	GLuint program_;


	static const std::string vertex_shader_source;
	static const std::string fragment_shader_source;


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

	void do_clear(
		const ClearFlags clear_flags) override
	{
		assert(is_context_current_);

		if (!is_initialized_ || !is_context_current_ || clear_flags == ClearFlags::none)
		{
			return;
		}

		auto ogl_clear_flags = GLbitfield{};

		if ((clear_flags & ClearFlags::color) != 0)
		{
			ogl_clear_flags |= GL_COLOR_BUFFER_BIT;
		}

		if ((clear_flags & ClearFlags::depth) != 0)
		{
			ogl_clear_flags |= GL_DEPTH_BUFFER_BIT;
		}

		if ((clear_flags & ClearFlags::stencil) != 0)
		{
			ogl_clear_flags |= GL_STENCIL_BUFFER_BIT;
		}

		::glClear(ogl_clear_flags);
		assert(ogl_is_succeed());
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

		screen_width_ = screen_width;
		screen_height_ = screen_height;

		// Get current context (must exist).
		//
		ogl_dc_ = ::wglGetCurrentDC();
		ogl_rc_ = ::wglGetCurrentContext();

		if (!ogl_dc_ || !ogl_rc_)
		{
			return false;
		}

		// Get implementation defined values.
		//
		::glGetIntegerv(GL_MAX_VIEWPORT_DIMS, max_viewport_size_.data());

		if (!ogl_is_succeed())
		{
			return false;
		}

		if (max_viewport_size_[0] <= 0 ||
			max_viewport_size_[1] <= 0 ||
			screen_width_ > max_viewport_size_[0] ||
			screen_height_ > max_viewport_size_[1])
		{
			return false;
		}

		// Load default program.
		//
		if (!load_program())
		{
			return false;
		}

		if (!enable_program(true))
		{
			return false;
		}


		// Set defaults.
		//
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

		if (program_)
		{
			::glUseProgram(0);
			::glDetachShader(program_, vertex_shader_);
			::glDetachShader(program_, fragment_shader_);
			::glDeleteProgram(program_);
			program_ = 0;
		}

		if (vertex_shader_)
		{
			::glDeleteShader(vertex_shader_);
			vertex_shader_ = 0;
		}

		if (fragment_shader_)
		{
			::glDeleteShader(fragment_shader_);
			fragment_shader_ = 0;
		}
	}


	void get_shader_build_status(
		const GLenum object_status,
		void (APIENTRYP ogl_get_parameter)(GLuint object, GLenum pname, GLint* params),
		void (APIENTRYP ogl_get_log)(GLuint object, GLsizei bufSize, GLsizei* length, GLchar* infoLog),
		const GLuint object,
		bool& is_built,
		std::string& log)
	{
		is_built = false;
		log.clear();

		auto ogl_compile_status = GLint{};
		auto ogl_log_length = GLint{};

		ogl_get_parameter(object, object_status, &ogl_compile_status);
		ogl_get_parameter(object, GL_INFO_LOG_LENGTH, &ogl_log_length);

		is_built = (ogl_compile_status == GL_TRUE);

		if (ogl_log_length > 0)
		{
			log.resize(ogl_log_length);
			ogl_get_log(object, ogl_log_length, &ogl_log_length, &log[0]);
		}
	}

	void get_shader_compile_status(
		const GLuint shader,
		bool& is_compiled,
		std::string& log)
	{
		get_shader_build_status(GL_COMPILE_STATUS, ::glGetShaderiv, ::glGetShaderInfoLog, shader, is_compiled, log);
	}

	void get_program_link_status(
		const GLuint program,
		bool& is_linked,
		std::string& log)
	{
		get_shader_build_status(GL_LINK_STATUS, ::glGetProgramiv, ::glGetProgramInfoLog, program, is_linked, log);
	}

	bool load_shader(
		const ShaderType shader_type,
		GLuint& shader)
	{
		shader = GL_NONE;

		const std::string* shader_source;
		GLenum ogl_shader_type;

		switch (shader_type)
		{
		case ShaderType::vertex:
			ogl_shader_type = GL_VERTEX_SHADER;
			shader_source = &vertex_shader_source;
			break;

		case ShaderType::fragment:
			ogl_shader_type = GL_FRAGMENT_SHADER;
			shader_source = &fragment_shader_source;
			break;

		default:
			return GL_NONE;
		}

		shader = ::glCreateShader(ogl_shader_type);

		const char* const source_lines[1] =
		{
			shader_source->c_str(),
		}; // source_lines

		const GLint source_lines_lengths[1] =
		{
			static_cast<GLint>(shader_source->length()),
		}; // source_lines

		::glShaderSource(shader, 1, source_lines, source_lines_lengths);
		::glCompileShader(shader);

		bool is_compiled;
		std::string log;

		get_shader_compile_status(shader, is_compiled, log);

		return is_compiled;
	}

	bool load_program()
	{
		if (!load_shader(ShaderType::vertex, vertex_shader_))
		{
			return false;
		}

		if (!load_shader(ShaderType::fragment, fragment_shader_))
		{
			return false;
		}

		program_ = ::glCreateProgram();

		if (!program_)
		{
			return false;
		}

		::glAttachShader(program_, vertex_shader_);
		::glAttachShader(program_, fragment_shader_);
		::glLinkProgram(program_);

		bool is_linked;
		std::string log;

		get_program_link_status(program_, is_linked, log);

		return is_linked;
	}

	bool enable_program(
		const bool is_enable)
	{
		::glUseProgram(is_enable ? program_ : 0);

		return ogl_is_succeed();
	}
}; // OglRendererImpl


const std::string OglRendererImpl::vertex_shader_source = std::string{
R"LTJ_VERTEX(

#version 330 core

layout(location = 0) in vec4 a_position;

void main()
{
	gl_Position = a_position;
	gl_Position.z = -gl_Position.z;
}

)LTJ_VERTEX"
}; // vertex_shader_source

const std::string OglRendererImpl::fragment_shader_source = std::string{
R"LTJ_FRAGMENT(

#version 330 core

out vec4 o_fragment;

void main()
{
	o_fragment = vec4(0, 1, 0, 1);
}

)LTJ_FRAGMENT"
}; // fragment_shader_source


OglRenderer::Viewport::Viewport()
	:
	x_{},
	y_{},
	width_{},
	height_{},
	depth_min_z_{},
	depth_max_z_{}
{
}


OglRenderer::OglRenderer()
{
}

OglRenderer::~OglRenderer()
{
}

bool OglRenderer::is_initialized() const
{
	return do_is_initialized();
}

bool OglRenderer::initialize(
	const int screen_width,
	const int screen_height)
{
	return do_initialize(screen_width, screen_height);
}

void OglRenderer::uninitialize()
{
	do_uninitialize();
}

void OglRenderer::set_current_context(
	const bool is_current)
{
	do_set_current_context(is_current);
}

void OglRenderer::set_clear_color(
	const std::uint8_t r,
	const std::uint8_t g,
	const std::uint8_t b,
	const std::uint8_t a)
{
	do_set_clear_color(r, g, b, a);
}

const OglRenderer::Viewport& OglRenderer::get_viewport() const
{
	return do_get_viewport();
}

void OglRenderer::clear(
	const ClearFlags clear_flags)
{
	do_clear(clear_flags);
}

void OglRenderer::set_viewport(
	const Viewport& viewport)
{
	do_set_viewport(viewport);
}

void OglRenderer::ogl_clear_error()
{
	static_cast<void>(ogl_is_succeed());
}

bool OglRenderer::ogl_is_succeed()
{
	auto is_failed = false;

	while (::glGetError() != GL_NO_ERROR)
	{
		is_failed = true;
	}

	return !is_failed;
}

OglRenderer& OglRenderer::get_instance()
{
	static OglRendererImpl instance{};
	return instance;
}


} // ltjs

