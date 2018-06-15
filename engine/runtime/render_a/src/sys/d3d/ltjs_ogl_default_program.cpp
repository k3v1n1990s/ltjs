#include "precompile.h"
#include "ltjs_ogl_default_program.h"
#include <cassert>
#include <utility>
#include "glad.h"


namespace ltjs
{


class OglDefaultProgram::Impl
{
public:
	Impl()
		:
		is_initialized_{},
		is_enabled_{},
		vertex_shader_{},
		fragment_shader_{},
		program_{}
	{
	}

	Impl(
		const Impl& that) = delete;

	Impl& operator=(
		const Impl& that) = delete;

	~Impl()
	{
		assert(program_ == 0);
	}


	bool api_initialize()
	{
		api_uninitialize();

		if (!load())
		{
			api_uninitialize();
			return false;
		}

		is_initialized_ = true;

		return true;
	}

	void api_uninitialize()
	{
		is_initialized_ = false;
		is_enabled_ = false;

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

	void api_enable(
		const bool is_enable)
	{
		if (!is_initialized_)
		{
			return;
		}

		is_enabled_ = is_enable;

		::glUseProgram(is_enabled_ ? program_ : 0);
	}

	int api_get_uniform_location(
		const char* const uniform_name) const
	{
		assert(is_enabled_);

		if (!is_initialized_ || !uniform_name)
		{
			return -1;
		}

		return ::glGetUniformLocation(program_, uniform_name);
	}

	void api_set_int(
		const GLint uniformat_location,
		const GLint value)
	{
		assert(is_enabled_);

		if (!is_initialized_ || !is_enabled_ || uniformat_location < 0)
		{
			return;
		}

		::glUniform1i(uniformat_location, value);
	}

	void api_set_uint(
		const GLint uniformat_location,
		const GLuint value)
	{
		assert(is_enabled_);

		if (!is_initialized_ || !is_enabled_ || uniformat_location < 0)
		{
			return;
		}

		::glUniform1ui(uniformat_location, value);
	}

	void api_set_vector(
		const GLint uniformat_location,
		const int item_count,
		const GLfloat* const items)
	{
		assert(is_enabled_);

		if (!is_initialized_ || !is_enabled_ || uniformat_location < 0 || !items)
		{
			return;
		}

		switch (item_count)
		{
		case 2:
			::glUniform2fv(uniformat_location, 1, items);
			break;

		case 3:
			::glUniform3fv(uniformat_location, 1, items);
			break;

		case 4:
			::glUniform4fv(uniformat_location, 1, items);
			break;

		default:
			return;
		}

	}

	void api_set_matrix4(
		const GLint uniformat_location,
		const GLfloat* const items)
	{
		assert(is_enabled_);

		if (!is_initialized_ || !is_enabled_ || uniformat_location < 0 || !items)
		{
			return;
		}

		::glUniformMatrix4fv(uniformat_location, 1, GL_TRUE, items);
	}


private:
	enum class ShaderType
	{
		vertex,
		fragment,
	}; // ShaderType


	static const std::string vertex_shader_source;
	static const std::string fragment_shader_source;


	bool is_initialized_;
	bool is_enabled_;

	GLuint vertex_shader_;
	GLuint fragment_shader_;
	GLuint program_;



	void get_build_status(
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
		get_build_status(GL_COMPILE_STATUS, ::glGetShaderiv, ::glGetShaderInfoLog, shader, is_compiled, log);
	}

	void get_program_link_status(
		const GLuint program,
		bool& is_linked,
		std::string& log)
	{
		get_build_status(GL_LINK_STATUS, ::glGetProgramiv, ::glGetProgramInfoLog, program, is_linked, log);
	}

	bool ogl_load_default_shader(
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

	bool load()
	{
		if (!ogl_load_default_shader(ShaderType::vertex, vertex_shader_))
		{
			return false;
		}

		if (!ogl_load_default_shader(ShaderType::fragment, fragment_shader_))
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
}; // OglDefaultProgram::Impl


const std::string OglDefaultProgram::Impl::vertex_shader_source = std::string{
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

const std::string OglDefaultProgram::Impl::fragment_shader_source = std::string{
R"LTJ_FRAGMENT(

#version 330 core

out vec4 o_fragment;

void main()
{
	o_fragment = vec4(0, 1, 0, 1);
}

)LTJ_FRAGMENT"
}; // fragment_shader_source


OglDefaultProgram::OglDefaultProgram()
	:
	impl_{std::make_unique<Impl>()}
{
}

OglDefaultProgram::OglDefaultProgram(
	OglDefaultProgram&& that)
	:
	impl_{std::move(that.impl_)}
{
}

OglDefaultProgram::~OglDefaultProgram()
{
}

bool OglDefaultProgram::initialize()
{
	return impl_->api_initialize();
}

void OglDefaultProgram::uninitialize()
{
	impl_->api_uninitialize();
}

void OglDefaultProgram::enable(
	const bool is_enable)
{
	impl_->api_enable(is_enable);
}

int OglDefaultProgram::get_uniform_location(
	const char* const uniform_name) const
{
	return impl_->api_get_uniform_location(uniform_name);
}

void OglDefaultProgram::set_bool(
	const int uniformat_location,
	const bool value)
{
	impl_->api_set_int(uniformat_location, value);
}

void OglDefaultProgram::set_int(
	const int uniformat_location,
	const int value)
{
	impl_->api_set_int(uniformat_location, value);
}

void OglDefaultProgram::set_uint(
	const int uniformat_location,
	const unsigned int value)
{
	impl_->api_set_uint(uniformat_location, value);
}

void OglDefaultProgram::set_vector(
	const int uniformat_location,
	const int item_count,
	const float* const values)
{
	impl_->api_set_vector(uniformat_location, item_count, values);
}

void OglDefaultProgram::set_vector2(
	const int uniformat_location,
	const float* const items)
{
	impl_->api_set_vector(uniformat_location, 2, items);
}

void OglDefaultProgram::set_vector3(
	const int uniformat_location,
	const float* const items)
{
	impl_->api_set_vector(uniformat_location, 3, items);
}

void OglDefaultProgram::set_vector4(
	const int uniformat_location,
	const float* const items)
{
	impl_->api_set_vector(uniformat_location, 4, items);
}

void OglDefaultProgram::set_matrix4(
	const int uniformat_location,
	const float* const values)
{
	impl_->api_set_matrix4(uniformat_location, values);
}

OglDefaultProgram &OglDefaultProgram::get_instance()
{
	static auto instance = OglDefaultProgram{};
	return instance;
}


} // ltjs
