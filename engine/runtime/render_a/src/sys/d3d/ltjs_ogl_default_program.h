#ifndef LTJS_OGL_DEFAULT_PROGRAM_INCLUDED
#define LTJS_OGL_DEFAULT_PROGRAM_INCLUDED


#include <memory>


namespace ltjs
{


class OglDefaultProgram
{
public:
	bool initialize();

	void uninitialize();


	void enable(
		const bool is_enable);

	int get_uniform_location(
		const char* const uniform_name) const;

	void set_bool(
		const int uniformat_location,
		const bool value);

	void set_int(
		const int uniformat_location,
		const int value);

	void set_uint(
		const int uniformat_location,
		const unsigned int value);

	void set_vector(
		const int uniformat_location,
		const int item_count,
		const float* const items);

	void set_vector2(
		const int uniformat_location,
		const float* const items);

	void set_vector3(
		const int uniformat_location,
		const float* const items);

	void set_vector4(
		const int uniformat_location,
		const float* const items);

	void set_matrix4(
		const int uniformat_location,
		const float* const items);


	static OglDefaultProgram &get_instance();


private:
	class Impl;

	using ImplUPtr = std::unique_ptr<Impl>;

	ImplUPtr impl_;


	OglDefaultProgram();

	OglDefaultProgram(
		const OglDefaultProgram& that) = delete;

	OglDefaultProgram& operator=(
		const OglDefaultProgram& that) = delete;

	OglDefaultProgram(
		OglDefaultProgram&& that);

	~OglDefaultProgram();
}; // OglDefaultProgram


} // ltjs


#endif // LTJS_OGL_DEFAULT_PROGRAM_INCLUDED
