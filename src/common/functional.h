// Copyright (c) 2014 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <boost/foreach.hpp>
#include <boost/variant/apply_visitor.hpp>
#include <boost/lexical_cast.hpp>

namespace tools
{
  struct identity
  {
    template<typename U>
    /* constexpr */ auto operator()(U&& v) const /* noexcept */
        -> decltype(std::forward<U>(v))
    {
        return std::forward<U>(v);
    }
  };
  
  template <typename Visitor>
  inline auto set_index(Visitor& v, int i, int which) -> decltype(v.set_index(i), void())
  {
    v.set_index(i);
  }
  template <typename Visitor>
  inline void set_index(Visitor& v, int i, long which)
  {
  }
  
  template <typename Visitor, typename Collection, class Mapper>
  bool all_apply_visitor(Visitor& v, const Collection& c, Mapper&& m, bool reverse=false)
  {
    if (reverse)
    {
      int i = c.size() - 1;
      BOOST_REVERSE_FOREACH(const auto& element, c)
      {
        set_index(v, i, 0);
        if (!boost::apply_visitor(v, m(element)))
          return false;
        i -= 1;
      }
    }
    else
    {
      int i = 0;
      BOOST_FOREACH(const auto& element, c)
      {
        set_index(v, i, 0);
        if (!boost::apply_visitor(v, m(element)))
          return false;
        i += 1;
      }
    }
    return true;
  }
  
  template <typename Visitor, typename Collection, class Mapper>
  bool all_apply_visitor(const Visitor& v, const Collection& c, Mapper&& m, bool reverse=false)
  {
    if (reverse)
    {
      BOOST_REVERSE_FOREACH(const auto& element, c)
      {
        if (!boost::apply_visitor(v, m(element)))
          return false;
      }
    }
    else
    {
      BOOST_FOREACH(const auto& element, c)
      {
        if (!boost::apply_visitor(v, m(element)))
          return false;
      }
    }
    return true;
  }
  
  template <typename Visitor, typename Collection, class Mapper>
  bool any_apply_visitor(Visitor& v, const Collection& c, Mapper&& m)
  {
    int i = 0;
    BOOST_FOREACH(const auto& element, c)
    {
      set_index(v, i, 0);
      if (boost::apply_visitor(v, m(element)))
        return true;
      i += 1;
    }
    return false;
  }
  
  template <typename Visitor, typename Collection, class Mapper>
  bool any_apply_visitor(const Visitor& v, const Collection& c, Mapper&& m)
  {
    BOOST_FOREACH(const auto& element, c)
    {
      if (boost::apply_visitor(v, m(element)))
        return true;
    }
    return false;
  }

  template <class Container, class MapF>
  std::string str_join(const Container& c, const MapF& f)
  {
    std::stringstream ss;
    
    bool first = true;
    BOOST_FOREACH(const auto& item, c)
    {
      if (!first)
      {
        ss << " ";
      }
      ss << boost::lexical_cast<std::string>(f(item));
      first = false;
    }
    
    return ss.str();
  }
  
  // MSVC 2012 prevents using default template args on functions, so:
  template <typename Visitor, typename Collection>
  bool all_apply_visitor(Visitor& v, const Collection& c)
  {
    return all_apply_visitor(v, c, identity());
  }
  template <typename Visitor, typename Collection>
  bool all_apply_visitor(const Visitor& v, const Collection& c)
  {
    return all_apply_visitor(v, c, identity());
  }
  template <typename Visitor, typename Collection>
  bool any_apply_visitor(Visitor& v, const Collection& c)
  {
    return any_apply_visitor(v, c, identity());
  }
  template <typename Visitor, typename Collection>
  bool any_apply_visitor(const Visitor& v, const Collection& c)
  {
    return any_apply_visitor(v, c, identity());
  }
  template <class Container>
  std::string str_join(const Container& c)
  {
    return str_join(c, identity());
  }
}
