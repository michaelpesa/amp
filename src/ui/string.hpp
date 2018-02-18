////////////////////////////////////////////////////////////////////////////////
//
// ui/string.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_7B3E67B2_E32E_4831_84B4_97686ACDBA65
#define AMP_INCLUDED_7B3E67B2_E32E_4831_84B4_97686ACDBA65


#include <amp/io/buffer.hpp>
#include <amp/stddef.hpp>
#include <amp/string_view.hpp>
#include <amp/type_traits.hpp>
#include <amp/u8string.hpp>

#include <cstddef>

#include <QtCore/QByteArray>
#include <QtCore/QMetaType>
#include <QtCore/QString>


Q_DECLARE_METATYPE(amp::u8string);


namespace amp {

inline auto to_u8string(QByteArray const& s)
{
    return u8string::from_utf8(s.data(), static_cast<std::size_t>(s.size()));
}

inline auto to_u8string(QString const& s)
{
    return u8string::from_utf16(reinterpret_cast<char16 const*>(s.data()),
                                static_cast<std::size_t>(s.size()));
}


template<typename T>
inline auto to_qstring(T const x, int const base = 10) ->
    enable_if_t<is_arithmetic_v<T>, QString>
{
    return QString::number(x, base);
}

inline auto to_qstring(QByteArray const& s)
{ return QString::fromUtf8(s); }

inline auto to_qstring(char const* const s)
{ return QString::fromUtf8(s); }

inline auto to_qstring(char const* const s, std::size_t const len)
{ return QString::fromUtf8(s, static_cast<int>(len)); }

inline auto to_qstring(string_view const s)
{ return QString::fromUtf8(s.data(), static_cast<int>(s.size())); }

inline auto to_qstring(char16 const* const s)
{ return QString::fromUtf16(s); }

inline auto to_qstring(char16 const* const s, std::size_t const len)
{ return QString::fromUtf16(s, static_cast<int>(len)); }

inline auto to_qstring(u16string_view const s)
{ return QString::fromUtf16(s.data(), static_cast<int>(s.size())); }

inline auto to_qbytearray(io::buffer const& buf)
{
    return QByteArray(reinterpret_cast<char const*>(buf.data()),
                      static_cast<int>(buf.size()));
}

}     // namespace amp


#endif  // AMP_INCLUDED_7B3E67B2_E32E_4831_84B4_97686ACDBA65

