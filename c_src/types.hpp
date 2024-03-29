#pragma once

#include <list>
#include <string>
#include <vector>
#include <boost/variant.hpp>
#include <boost/mpl/identity.hpp>

#include <erl_nif.h>

/////////////////////////////////////////////////////////////////////////////

namespace erlcpp {

/////////////////////////////////////////////////////////////////////////////

typedef boost::variant
<
    int32_t,
    int64_t,
    double
> num_t;

class lpid_t;
class atom_t;
class binary_t;
class list_t;
class tuple_t;

typedef boost::variant
<
    num_t,
    lpid_t,
    atom_t,
    binary_t,
    boost::recursive_wrapper<list_t>,
    boost::recursive_wrapper<tuple_t>
> term_t;

class lpid_t
{
public :
    ErlNifPid* ptr() { return &pid; }
    ErlNifPid const* ptr() const { return &pid; }
    lpid_t(ErlNifPid pid) : pid(pid) {}
    lpid_t() {}
private :
    ErlNifPid pid;
};

class atom_t : public std::string
{
public :
    typedef std::string data_t;
    atom_t() {};
    explicit atom_t(char   const* str) : data_t(str) {};
    explicit atom_t(data_t const& str) : data_t(str) {};
};

class binary_t : public std::vector<char>
{
public :
    typedef std::vector<char> data_t;
    binary_t() {};
    explicit binary_t(data_t const& val) : data_t(val) {};
    explicit binary_t(std::string const& val) : data_t(val.begin(), val.end()) {};
};

class list_t : public std::list<term_t>
{
public :
    typedef std::list<term_t> data_t;
    list_t() {};
    explicit list_t(data_t const& val) : data_t(val) {};
};

class tuple_t : public std::vector<term_t>
{
public :
    typedef std::vector<term_t> data_t;
    tuple_t() {};
    explicit tuple_t(data_t const& val) : data_t(val) {};
    explicit tuple_t(data_t::size_type sz) : data_t(sz) {};
};

/////////////////////////////////////////////////////////////////////////////

};
