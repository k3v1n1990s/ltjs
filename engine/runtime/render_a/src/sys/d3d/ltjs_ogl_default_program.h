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
