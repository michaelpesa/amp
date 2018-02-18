////////////////////////////////////////////////////////////////////////////////
//
// core/registry.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_0FD0ED70_DF0F_4FC3_9BCA_214C12F07BFD
#define AMP_INCLUDED_0FD0ED70_DF0F_4FC3_9BCA_214C12F07BFD


#include <amp/audio/filter.hpp>
#include <amp/audio/output.hpp>
#include <amp/intrusive/set.hpp>
#include <amp/intrusive/slist.hpp>
#include <amp/stddef.hpp>


namespace amp {

class u8string;

void load_plugins();


namespace audio {

extern intrusive::set<audio::output_session_factory> output_factories;
extern intrusive::set<audio::filter_factory> filter_factories;
extern intrusive::slist<audio::resampler_factory> resampler_factories;

u8string get_input_file_filter();
bool have_input_for(u8string const&) noexcept;

}}    // namespace amp::audio


#endif  // AMP_INCLUDED_0FD0ED70_DF0F_4FC3_9BCA_214C12F07BFD

