#ifndef TAO_FORWARD_H
#define TAO_FORWARD_H

#ifndef TAOCPP_JSON_INCLUDE_JSON_HPP
#define TAOCPP_JSON_INCLUDE_JSON_HPP

typedef unsigned char byte;

#include <tao/json/external/optional.hpp>
#include <tao/json/single.hpp>
#include <tao/json/traits.hpp>

namespace tao { namespace json {
    template< template< typename ... > class Traits >
    class basic_value;
    template< typename T, typename >
    struct traits;
    using value = basic_value<traits>;
    using array_t = std::vector< value >;
    using object_t = std::map< std::string, value >;
    typedef std::initializer_list< array_t::value_type > array_initializer;
    typedef std::initializer_list< object_t::value_type > object_initializer;
} }

#endif // TAOCPP_JSON_INCLUDE_JSON_HPP

namespace tao { namespace json {
    using array_t = std::vector< value >;
    using object_t = std::map< std::string, value >;
    typedef std::initializer_list< array_t::value_type > array_initializer;
    typedef std::initializer_list< object_t::value_type > object_initializer;
} }

#endif // TAO_FORWARD_H
