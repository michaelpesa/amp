////////////////////////////////////////////////////////////////////////////////
//
// audio/filter_chain.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/audio/filter.hpp>
#include <amp/audio/format.hpp>
#include <amp/audio/packet.hpp>
#include <amp/error.hpp>
#include <amp/range.hpp>
#include <amp/stddef.hpp>

#include "audio/channel_mixer.hpp"
#include "audio/filter_chain.hpp"
#include "core/registry.hpp"

#include <memory>
#include <utility>


namespace amp {
namespace audio {
namespace {

auto make_resampler(audio::format& src, audio::format const& dst,
                    uint8 const quality = audio::quality_medium)
{
    std::exception_ptr ep;
    for (auto&& factory : audio::resampler_factories) {
        try {
            auto instance = factory.create();
            instance->set_sample_rate(dst.sample_rate);
            instance->set_quality(quality);
            instance->calibrate(src);
            return instance;
        }
        catch (...) {
            ep = std::current_exception();
        }
    }

    if (ep) {
        std::rethrow_exception(ep);
    }
    raise(errc::failure, "no audio resampler plugin");
}

}     // namespace <unnamed>


void filter_chain::rebuild(std::vector<u8string> const& preset,
                           audio::replaygain_config const& config)
{
    for (auto&& id : preset) {
        auto factory = audio::filter_factories.find(id);
        if (factory != audio::filter_factories.end()) {
            elems_.push_back(factory->create());
        }
    }
    rgain_.reset(config);
}

void filter_chain::calibrate(audio::format const& src,
                             audio::format const& dst,
                             audio::replaygain_info const& info)
{
    auto fmt = src;
    fmt.validate();

    for (auto&& elem : elems_) {
        elem->calibrate(fmt);
        fmt.validate();
    }

    if (fmt.channel_layout != dst.channel_layout) {
        elems_.push_back(channel_mixer::make(fmt, dst));
    }
    if (fmt.sample_rate != dst.sample_rate) {
        elems_.push_back(make_resampler(fmt, dst));
    }
    rgain_.calibrate(info);
}

void filter_chain::process(audio::packet& pkt)
{
    for (auto&& elem : elems_) {
        elem->process(pkt);
    }
    rgain_.process(pkt);
}

void filter_chain::drain(audio::packet& pkt)
{
    audio::packet tmp;
    tmp.set_channel_layout(pkt.channel_layout(), pkt.channels());

    auto const last = elems_.end();
    for (auto first = elems_.begin(); first != last; ) {
        (*first++)->drain(tmp);

        if (!tmp.empty()) {
            for (auto next = first; next != last; ++next) {
                (*next)->process(tmp);
            }
            pkt.append(tmp.cbegin(), tmp.cend());
            tmp.clear();
        }
    }
    rgain_.process(pkt);
}

void filter_chain::flush()
{
    for (auto&& elem : elems_) {
        elem->flush();
    }
}

}}    // namespace amp::audio

