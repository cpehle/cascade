/*
Copyright 2007, D. E. Shaw Research.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above copyright
  notice, this list of conditions, and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions, and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

* Neither the name of D. E. Shaw Research nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

//////////////////////////////////////////////////////////////////////
//
// Parameter.hpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 02/21/2007
//
// Generic parameter class.  Specific parameter types are defined
// at the end of the file.  
//
// +--------------------+
//  Declaring parameters
// +--------------------+
// 
// Parameters declared in header files must be contained within a singleton 
// object as shown in the following example:
//
// ModelParams.hpp:
//
//    struct ModelParams
//    {
//        IntParameter    (NumCompanies,       20);
//        BoolParameter   (UseSimpleModel,     false,
//                        "If true, ignore all second order Lippert-Xu effects");
//        StringParameter (Continent,          "North America",
//                        "Select the continent on which to run the model",
//                        "North America", "Asia", "Europe");
//    };
//
//    extern ModelParams modelParams;
//
// ModelParams.cpp:
//
//    ModelParams modelParams;
//
// The example also illustrates the three ways in which a parameter
// can be declared:
//
//    1) Name and default value only
//    2) Name, default value and description
//    3) Name, default value, description, and a set of legal options
//
// Parameters declared in a source file can be declared anywhere at file scope.
//
// +----------------------+
//  Enumeration parameters
// +----------------------+
//
// An enumeration parameter is declared using the following macro:
//
//    EnumParameter(name, default, description, value*)
//
// This macro both declares a parameter with the specified name, and creates an 
// enumeration type with the specified values.  The name of the enumeration type 
// is obtained by prepending 'E' to the parameter name.  For example:
//
//    struct NetParams
//    {
//        EnumParameter(OnChipNetwork, Mesh, "Specifies the on-chip network model",
//                      Mesh, Crossbar);
//    };
//
// The OnChipNetwork parameter then has type NetParams::EOnChipNetwork, which 
// has values NetParams::Mesh and NetParams::Crossbar.  Note that for enumeration 
// parameters, the description and set of legal options must be supplied.
//
// +--------------------+
//  Accessing parameters
// +--------------------+
//
// For the most part, parameters behave exactly like the types they contain.
// They can be assigned to directly, and they will be automatically cast to
// the appropriate type.  To explicitly cast a parameter, use the dereference
// operator (*) as follows:
//
//    log("Number of companies: %d\n", *modelParams.NumCompanies);
//
// If a parameter has member functions/variables, they can be accessed using
// the pointer dereference operator (->) as follows:
//
//    log("Continent = %s\n", *modelParams.Continent);
//
// The Parameter base class also provides three static functions to access 
// parameters by name:
//
//    bool Parameter::setValueByString (const string &name, const string &val);
//    string Parameter::getValueAsString (const string &name);
//    void Parameter::help (const string &include = "*", const string &exclude = "");
//
// +----------------+
//  Parameter groups
// +----------------+
//
// Parameters can be placed within a parameter group by declaring them within a
// structure and then instantiating the structure with the ParameterGroup macro:
//
//    ParameterGroup(type, name, ["group_name"])
//
// Within the C++ code, this is logically equivalent to the declaration
//
//    type name;
//
// and in particular parameters within the structure are accessed as 
// name.<parameter>.  The macro has the effect of placing these parameters 
// within a named parameter group.  If only two arguments are provided to
// ParameterGroup(), then the group name is simply "name" (i.e. the 
// stringification of the second argument).  Alternately, the group name can
// be explicitly specified using the optional third string argument.
//
// Placing parameters within a group automatically prefixes their string names 
// with "group_name.".  It does not change their behaviour when they are 
// accessed directly from C++ code.
//
// The 'static' and 'extern' modifiers can be placed in front of the
// ParameterGroup macro and they will have the usual meaning.  So, for example,
// a ParameterGroup declared as static within a source file will not be visible
// outside of that source file, and a ParameterGroup declared in a header file
// should be declared 'extern'.  In the later case, the actual declaration of
// the parameter group should use the macro DeclareParameterGroup(name), e.g.:
//
//    extern ParameterGroup(NetworkParametres, netParams, "net");
//    DeclareParameterGroup(netParams);
//
// Parameter groups can be nested, as illustrated in the following example:
//
//    struct ArbiterParams
//    {
//        BoolParameter(RoundRobin, true);
//        BoolParameter(RollingUpdate, true);
//    };
//
//    struct RouterParams
//    {
//        IntParameter(InputQueueDepth, 4);
//        IntParameter(Latency, 4);
//
//        ParameterGroup(ArbiterParams, arb);
//    };
//
//    extern ParameterGroup(RouterParams, routerParams, "router");
//
// This example defines four parameters whose C++ names and string names are as 
// follows:
//
// - routerParams.InputQueueDepth   ("router.InputQueueDepth")
// - routerParams.Latency           ("router.Latency")
// - routerParams.arb.RoundRobin    ("router.arb.RoundRobin")
// - routerParams.arb.RollingUpdate ("router.arb.RollingUpdate")
//
// +----------------------------+
//  Creating New Parameter Types
// +----------------------------+
//
// To define a new parameter type, define a new parameter macro following
// the example of the macros at the end of this file, then implement the
// functions which convert values of that type to and from strings if they
// are not already implemented (see strcast.hpp), and the equality operator ==
// for that type.  Any enumeration created using DECLARE_ENUMERATION can be
// used as a parameter type.
//
// If a parameter type has a multi-argument constructor, e.g.
//
//    struct Complex
//    {
//        Complex () {}
//        Complex (float _r, float _i) : r(_r), i(_i) {}
//        float r, i;
//    };
//
// then you can define the parameter macro as follows:
//
//    #define ComplexParameter(name, default, ...)
//        Parameter(Complex, name, Complex default, ##__VA_ARGS__)
//
// This allows parameters of this type to be declared by supplying constructor 
// arguments:
//
//        ComplexParameter (JuliaC,    (-.25, -.25),
//                         "C value for Julia set");
//
// Valid options for parameters with multiple constructor arguments must be
// specified as strings.
//
// Defining a new parameter macro is optional, as parameters of arbitrary
// type can still be instantiated using the generic Parameter() macro directly,
// e.g.:
//
//        Parameter(Complex, JuliaC, "(-.25, -.25)",
//                           "C value for Julia set");
//
//////////////////////////////////////////////////////////////////////

#ifndef descore_Parameter_hpp_
#define descore_Parameter_hpp_

#include "descore.hpp"
#include "Enumeration.hpp"
class Archive;

/////////////////////////////////////////////////////////////////
//
// ParamMap typedefs
//
/////////////////////////////////////////////////////////////////
class Parameter;
typedef std::map<string, Parameter *> ParamMap;

////////////////////////////////////////////////////////////////////////////////
//
// Callback for a parameter that changes value
//
////////////////////////////////////////////////////////////////////////////////
struct IParameterChangeCallback
{
    virtual void notifyChange (Parameter *param) = 0;
};

//Used to store an explicit checkpoint of parameter state
struct ParameterCheckPointState {
    std::map<string, bool>   m_checkpointModified;
    std::map<string, string> m_checkpointValue;
};

/////////////////////////////////////////////////////////////////
//
// Base Parameter class
//
/////////////////////////////////////////////////////////////////
class Parameter
{
    DECLARE_NOCOPY(Parameter);
public:
    Parameter () : m_hidden(false) {}
    virtual ~Parameter () {}

    // Called from the constructor to set the parameter name with the appropriate group prefix
    void setName (const string &name);

    // Called from the constructor and possibly once before the constructor if the parameter
    // is accessed at static initialization time.  Returns true if the parameter is already
    // initialized.
    bool initialize (const char *type, const char *description, const char *file, int line);

    //----------------------------------
    // Parsing callback interface
    //----------------------------------
    struct IParseCallback
    {
        virtual bool parseLine (const char *line) = 0;
    };

    //----------------------------------
    // Static (global) functions
    //----------------------------------

    // Parse any '-parameter=value' or '-bool_parameter' directives from the 
    // command-line arguments and remove them from the command line arguments.  
    // Also recognizes the following:
    //
    //   -quiet                alias for 'log.quiet'
    //   -showparams [pattern] lists all matching parameters and exits
    //   -showconfig [pattern] shows matching parameters in hierarchical config-file format
    //   -loadparams <file>    loads parameter settings from the specified file
    //
    // A directive that does not match the name of any parameter is ignored.  
    // The following forms may also be used to force the directive to be 
    // processed as a parameter:
    //
    //   -setparam parameter=value    (or -setparam bool_parameter)
    //   -tryparam parameter=value    (or -tryparam bool_parameter)
    //
    // For -setparam, if the directive does not match the name of any parameter
    // then an error is generated.  For -tryparam, if the directive does not
    // match the name of any parameter then it is silently ignored and removed
    // from the command-line arguments.
    //
    static void parseCommandLine (int &csz, char *rgsz[]);

    // Parse parameter settings of the form 'parameter = value' from a file.
    // Parameter settings can be hierarchical, e.g.
    //
    //   router
    //   {
    //       InputQueueDepth = 4
    //       Latency = 5
    //   }
    //
    // is the same as
    //
    //   router.InputQueueDepth = 4
    //   router.Latency = 5
    //
    // Comments begin with '//'.  Unrecognized lines are passed to the callback
    // function for parsing, which should return true for success and false
    // if the line could not be parsed.
    static void parseFile (const string &filename, IParseCallback *callback = NULL);

    // Generate a parameter settings file that includes the values of all matching parameters.
    // 'include' is a specifier string defining a list of include patterns;
    // 'exclude' is a specifier string defining a list of expclude patterns.
    // A parameter is included if it matches one of the include patterns but
    // does not match any of the exclude patterns.  A parameter matches a pattern
    // if some subset of its hierarchical name matches the pattern, e.g.
    // the parameter "foo.bar" matches a pattern if one of "foo", "bar", or "foo.bar"
    // matches.
    static void generateFile (const string &filename, const string &include = "*", const string &exclude = "");

    // Set a single parameter by name.  Return true if successful,
    // false on failure.  A failure indicates that either the parameter
    // does not exist, or the value is not one of the legal options.
    static bool setValueByString (const string &name, const string &val, bool logerrors = true);

    // Get the string representation of a parameter by name.
    static string getValueAsString (const string &name);

    // Convenience wrapper to set a value as a specific type
    template <typename T>
    static bool setValue (const string &name, const T &val, bool logerrors = true)
    {
        return setValueByString(name, str(val), logerrors);
    }

    // Convenience wrapper to get the value as a specific type
    template <typename T>
    static T getValue (const string &name) 
    {
        return stringTo<T>(getValueAsString(name));
    }

    // Display help information for all matching parameters on stdout.
    // 'include' is a specifier string defining a list of include patterns;
    // 'exclude' is a specifier string defining a list of exclude patterns.
    // A parameter is included if it matches one of the include patterns but
    // does not match any of the exclude patterns.  A parameter matches a pattern
    // if some subset of its hierarchical name matches the pattern, e.g.
    // the parameter "foo.bar" matches a pattern if one of "foo", "bar", or "foo.bar"
    // matches.  If 'brief' is true, then help information is shown in a compact 
    // configuration file format.
    static void help (const string &include = "*", const string &exclude = "", bool brief = false);

    // Archive all parameters
    static void archiveParameters (Archive &ar, const string &include = "*", const string &exclude = "");

    // Reset all matching parameters to their default value
    static void resetParameters (const string &include = "*", const string &exclude = "");

    // Create a snapshot of the current parameter state in "state"
    static void checkpointParameters (ParameterCheckPointState &state,
                                      const string &include = "*", const string &exclude = "");

    // Revert all parameter values to the checkpoint state passed in
    static void restoreParametersFromCheckpoint (ParameterCheckPointState &state,
                                                 const string &include = "*", const string &exclude = "");

    // Return a flattened map containing all matching parameters
    static ParamMap getParameters (const string &include, const string &exclude = "");
    static const ParamMap &getParameters ();

    // Helper fuction to retrieve a parameter by name; returns NULL if the parameter does not exist
    static Parameter *findParameter (const string &name);

protected:
    // Call registered callbacks when the value changes
    void notifyChange ();

public:
    //----------------------------------
    // Member accessors
    //----------------------------------

    // Get the string representation of the parameter's current value
    virtual string getValueAsString () const = 0;

    // Get the string representation of the parameter's default value
    virtual string getDefaultAsString () const = 0;

    // Get the string representatin of the parameter's legal options, or "" if none are specified
    string getOptionsAsString () const;

    // Get the parameter name/type/description
    string getName () const
    {
        return m_name;
    }
    string getType () const
    {
        return m_type;
    }
    string getDescription () const
    {
        return m_description;
    }
    void appendDescription (const string &s)
    {
        m_description += s;
    }
    string getFile () const
    {
        return m_file;
    }
    int getLine () const
    {
        return m_line;
    }

    // Returns true if the parameter's value has been set explicitly.
    bool modified () const
    {
        return m_modified;
    }

    // Returns true if the parameter does not appear in help output
    bool hidden () const
    {
        return m_hidden;
    }

    // Add a callback for when the value changes
    void addCallback (IParameterChangeCallback *callback);

    // Print the help string, nicely formatted
    void printDescription (int indent)
    {
        printHelpString(*m_description, indent);
    }

private:
    // Internal functions for accessing options
    virtual int numOptionsInternal () const = 0;
    virtual string getOptionAsString (int i) const = 0;

public:
    // Helper function used during construction to define parameter group names.  
    // If 'update' is false, returns the current full hierarchical group name.  
    // If 'update' is true and 'name' is "", ends the current group.  If 'update' 
    // is true and 'name' is not "", starts a new group named "name".
    static string parameterGroup (bool update = false, const string &name = "");

protected:

    // Set the parameter value from a string with optional indexing
    virtual bool setValueInternal (const string &val, const string &index = "", bool logerrors = true) = 0;

    // Functions used to implement 'help'
    void helpInternal (const char *fmt);
    void printHelpString (const char *sz, int indent);

    // Functions implemented by type-specific parameters
    virtual void reset () = 0;

    // Support for parameter checkpointing
    void createCheckpoint ();
    void restoreFromCheckpoint ();

protected:
    string     m_name;         // Parameter name
    const char *m_type;        // Parameter type as string
    string     m_description;  // Parameter help string
    const char *m_file;        // File name where parameter is defined
    int        m_line;         // Line number within file where parameter is defined
    bool       m_modified;     // Indicates that the parameter has been set explicitly
    bool       m_hidden;       // Do not print help information for this parameter
    std::vector<IParameterChangeCallback *> m_callbacks; // Callbacks for when the value changes
};

/////////////////////////////////////////////////////////////////
//
// Union of string and parameter value used make the vararg 
// parameter macros work and to allow paramter values to be 
// specified directly or as strings.
//
/////////////////////////////////////////////////////////////////

// Helper struct to define a secondary conversion from int to avoid
// abiguous conversion errors when the arg is 0 and T is not int.
template <typename T> struct ParameterArgInt
{
    typedef int int_t;
};

// Disable the secondary conversion for certain specific types
struct ParameterArgInvalid;
template <> struct ParameterArgInt<int>
{
    typedef ParameterArgInvalid int_t;
};
template <> struct ParameterArgInt<float>
{
    typedef ParameterArgInvalid int_t;
};

// Helper struct to map the parameter type to the literal value type.  Usually they're
// the same, but for descore enumerations the literal value type is T::enum_t
template <typename T, bool is_enum>
struct ParameterValueTypeHelper
{
    typedef T value_t;
};
template <typename T>
struct ParameterValueTypeHelper<T, true>
{
    typedef typename T::enum_t value_t;
};
template <typename T>
struct ParameterValueType
{
    typedef typename ParameterValueTypeHelper<T, __is_descore_enumeration(T)>::value_t value_t;
};

template <typename T>
struct ParameterArg
{
    ParameterArg (const char *_sz) : sz(_sz)
    {
    }
    ParameterArg (const typename ParameterValueType<T>::value_t &_value) : sz(NULL), value(_value)
    {
    }
    ParameterArg (typename ParameterArgInt<T>::int_t _value) : sz(NULL), value(_value)
    {
    }
    T &getValue ()
    {
        if (sz)
        {
            try
            {
                fromString(value, sz);
            }
            catch (strcast_error &e)
            {
                die("%s", e.what());
            }
        }
        return value;
    }

    const char *sz;
    T value;
};

/////////////////////////////////////////////////////////////////
//
// String conversion used for parameter (defaults to string_cast).
// The default string conversion for float/double is problematic
// because it gives different results on different architectures.
// So for parameters, restrict the precision with which float/
// double paramter values are displayed as strings.
//
/////////////////////////////////////////////////////////////////
template <typename T>
struct ParameterStringCast : public descore::string_cast<T>
{
};

template <>
struct ParameterStringCast<float> : public descore::string_cast<float>
{
    static string str (const float &val)
    {
        return ::str("%.6g", val);
    }
};

template <>
struct ParameterStringCast<double> : public descore::string_cast<double>
{
    static string str (const double &val)
    {
        return ::str("%.12lg", val);
    }
};

////////////////////////////////////////////////////////////////////////////////
//
// Helper struct/function for setting indexed values within parameters
//
////////////////////////////////////////////////////////////////////////////////

BEGIN_NAMESPACE_DESCORE
template <typename T> bool setParameterValue (T &val, const string &index, const string &sval, string *error_message);
template <typename T> struct vec3;
END_NAMESPACE_DESCORE

//----------------------------------
// Type-specific indexing
//----------------------------------

template <typename T>
struct ParameterIndexer
{
    static bool setParameterValue (T &, const string &, const string &, const string &, string *error_message)
    {
        if (error_message)
            *error_message = str("Value of type %s cannot be indexed", descore::type_traits<T>::name());
        return false;
    }
};

template <typename T, typename Tidx = int>
struct ParameterGenericIndexer
{
    static bool setParameterValue (T &val, const string &index0, const string &index1, const string &sval, string *error_message)
    {
        Tidx idx;
        if (!tryFromString(idx, index0))
        {
            if (error_message)
                *error_message = str("Could not parse index [%s] as %s", *index0, descore::type_traits<Tidx>::name());
            return false;
        }
        if (!ParameterIndexer<T>::validateIndex(val, idx))
        {
            if (error_message)
                *error_message = str("Parameter index %s is out of range", *str(idx));
            return false;
        }
        return descore::setParameterValue(val[idx], index1, sval, error_message);
    }
};

template <typename T>
struct ParameterIndexer<descore::vec3<T> > : public ParameterGenericIndexer<descore::vec3<T> >
{
    static bool validateIndex (descore::vec3<T> &, unsigned idx)
    {
        return idx < 3;
    }
};

template <typename T, typename A>
struct ParameterIndexer<std::vector<T, A> > : public ParameterGenericIndexer<std::vector<T, A> >
{
    static bool validateIndex (std::vector<T, A> &vec, unsigned idx)
    {
        return idx < vec.size();
    }
};

template <typename K, typename T, typename P, typename A>
struct ParameterIndexer<std::map<K, T, P, A> > : public ParameterGenericIndexer<std::map<K, T, P, A>, K>
{
    static bool validateIndex (std::map<K, T, P, A> &, K &)
    {
        return true;
    }
};

//----------------------------------
// Generic set value with indexing
//----------------------------------

BEGIN_NAMESPACE_DESCORE

bool splitIndex (const string &index, string &index0, string &index1, string *error_message);

template <typename T>
bool setParameterValue (T &val, const string &index, const string &sval, string *error_message)
{
    typedef ParameterStringCast<T> strcast;
    if (index == "")
        return tryFromString(val, sval, error_message);
    string index0, index1;
    if (!splitIndex(index, index0, index1, error_message))
        return false;
    return ParameterIndexer<T>::setParameterValue(val, index0, index1, sval, error_message);
}

END_NAMESPACE_DESCORE

/////////////////////////////////////////////////////////////////
//
// Parameter with value
//
/////////////////////////////////////////////////////////////////

template <typename T>
struct ParameterVectorElementType
{
    typedef T elem_t;
};

template <typename T>
struct ParameterVectorElementType<std::vector<T> >
{
    typedef T elem_t;
};

template <typename T>
class ParameterValue : public Parameter
{
    typedef ParameterStringCast<T> strcast;

public:
    typedef void (*GetInfoFunc) (ParameterArg<T> *&, int &, const char *&, const char *&, const char *&, int &);

    ParameterValue (GetInfoFunc getInfo)
    {
        initializeInternal(true, getInfo);
    }

    // Direct accessors
    inline operator typename ParameterValueType<T>::value_t () const 
    { 
        validate();
        return this->m_val; 
    }
    inline T *operator-> ()
    { 
        validate();
        return &this->m_val; 
    }
    inline T &operator* ()
    { 
        validate();
        return this->m_val; 
    }
    inline const T *operator-> () const
    { 
        validate();
        return &this->m_val; 
    }
    inline const T &operator* () const
    { 
        validate();
        return this->m_val; 
    }
    inline T &operator= (const T &rhs)
    {
        validate();
        return ParameterValue<T>::assign(rhs);
    }
    inline T &operator= (const ParameterValue<T> &rhs)
    {
        validate();
        return *this = *rhs;
    }

    // Array accessor for when the parameter type is an std::vector
    inline typename ParameterVectorElementType<T>::elem_t &operator[] (int index)
    {
        validate();
        return this->m_val[index];
    }
    inline const typename ParameterVectorElementType<T>::elem_t &operator[] (int index) const
    {
        validate();
        return this->m_val[index];
    }

    // Default accessors
    T getDefault () const
    {
        validate();
        return this->m_default;
    }
    void setDefault (const T &val) 
    {
        validate();
        this->m_default = val;
        if (!this->m_modified)
            this->reset();
    }

    // Options accessors
    inline int numOptions () const
    {
        validate();
        return (int) this->m_options.size();
    }
    inline const T &getOption (int i) const
    {
        validate();
        return this->m_options[i];
    }

    // Reset
    virtual void reset ()
    {
        validate();
        this->m_modified = false;
        if (!(m_val == m_default))
        {
            m_val = m_default;
            this->notifyChange();
        }
    }

    // AsString accessors
    virtual string getDefaultAsString () const 
    { 
        validate();
        return strcast::str(m_default); 
    }
    virtual string getValueAsString () const 
    { 
        validate();
        return strcast::str(this->m_val); 
    }

protected:
    void initializeInternal (bool constructor, GetInfoFunc getInfo)
    {
        ParameterArg<T> *args = NULL;
        int numArgs           = 0;
        const char *type      = NULL;
        const char *name      = NULL;
        const char *file      = NULL;
        int line              = 0;
        getInfo(args, numArgs, type, name, file, line);

        if (constructor)
            Parameter::setName(name);
        else
            Parameter::m_name = name;

        if (Parameter::initialize(type, (numArgs > 1) ? args[1].sz : NULL, file, line))
            return;
        m_initialized = true;

        for (int i = 2 ; i < numArgs ; i++)
            m_options.push_back(args[i].getValue());

        // If there are no explicitly-specified options, then check to see if this
        // is an enumeration.
        if (!m_options.size() && __is_descore_enumeration(T))
        {
            const descore::EnumerationType *type = descore::getEnumerationType<T>();
            for (int i = 0 ; i <= type->maxValue() ; i++)
            {
                if (type->isValid(i))
                    m_options.push_back(stringTo<T>(type->getSymbol(i)));
            }
        }

        // Validate the default value
        m_default = args->getValue();
        if (!setValueInternal(strcast::str(m_default)))
            logerr("Warning: Initializing %s to invalid value %s\n", *Parameter::m_name, *getValueAsString());
        m_val = m_default;
        this->m_modified = false;
    }

    // Return true on success, false otherwise
    virtual bool setValueInternal (const string &val, const string &index = "", bool logerrors = true)
    {
        T tval = m_val;
        string error_message;
        if (!descore::setParameterValue(tval, index, val, &error_message))
        {
            if (logerrors)
            {
                logerr("Cannot set %s%s to %s\n    %s\n", *Parameter::m_name, *index, *val, *error_message);
                if (__is_descore_enumeration(T))
                {
                    const descore::EnumerationType *type = descore::getEnumerationType<T>();
                    logerr("Valid values for %s are: %s.\n", *Parameter::m_name, *type->getValuesAsString());
                }
            }
            return false;
        }
        if (m_options.size())
        {
            bool match = false;
            for (unsigned i = 0 ; !match && (i < m_options.size()) ; i++)
                match = (tval == m_options[i]);
            if (!match)
            {
                if (logerrors)
                {
                    logerr("Cannot set %s%s to %s.\n", *Parameter::m_name, *index, *val);
                    logerr("Valid values for %s are: %s.\n", *Parameter::m_name, *Parameter::getOptionsAsString());
                }
                return false;
            }
        }
        assign(tval);
        return true;
    }

    // Assignment
    inline T &assign (const T &rhs)
    {
        validate();
        this->m_modified = true;
        if (!(m_val == rhs))
        {
            m_val = rhs;
            this->notifyChange();
        }
        return m_val;
    }

    // Make sure we're initialized
    void validate () const
    {
        assert(m_initialized, "Unsupported Parameter access at static initialization time");
    }

private:
    virtual int numOptionsInternal () const 
    { 
        return (int) m_options.size(); 
    }
    virtual string getOptionAsString (int i) const 
    { 
        return strcast::str(m_options[i]); 
    }

protected:
    // For statically scoped parameters, we rely on m_initialized to be zero-initialized
    // so that attempts to access the parameter at static initialization time before it
    // has been constructed will behave properly.
    bool m_initialized;
    T    m_val;
    T    m_default;
    std::vector<T> m_options;
};

template <typename T, typename TP>
class SafeParameterValue : public ParameterValue<T>
{
    typedef ParameterStringCast<T> strcast;
public:
    SafeParameterValue () : ParameterValue<T>(&TP::getInfo)
    {
    }

    // Direct accessors
    inline operator typename ParameterValueType<T>::value_t () const 
    { 
        initialize ();
        return this->m_val;
    }
    inline T *operator-> ()
    { 
        initialize();
        return &this->m_val;
    }
    inline T &operator* ()
    { 
        initialize();
        return this->m_val;
    }
    inline const T *operator-> () const
    { 
        initialize();
        return &this->m_val;
    }
    inline const T &operator* () const
    { 
        initialize();
        return this->m_val;
    }
    inline T &operator= (const T &rhs)
    {
        initialize();
        return ParameterValue<T>::assign(rhs);
    }
    inline T &operator= (const ParameterValue<T> &rhs)
    {
        return *this = *rhs;
    }

    // Array accessor for when the parameter type is an std::vector
    inline typename ParameterVectorElementType<T>::elem_t &operator[] (int index)
    {
        initialize();
        return this->m_val[index];
    }
    inline const typename ParameterVectorElementType<T>::elem_t &operator[] (int index) const
    {
        initialize();
        return this->m_val[index];
    }

protected:
    // Initialization
    inline void initialize () const
    {
        // Minor "mutable" hack to allow initialize() to be called from
        // a const member function.
        if (!this->m_initialized)
            ((SafeParameterValue *) this)->initializeInternal(false, &TP::getInfo);
    }
};

/////////////////////////////////////////////////////////////////
//
// Parameter declaration
//
/////////////////////////////////////////////////////////////////
#define Parameter(type, name, default, ...) \
struct Parameter__##name : public SafeParameterValue<type, Parameter__##name> \
    { \
        using SafeParameterValue<type, Parameter__##name>::operator=; \
        static void getInfo (ParameterArg<type> *&_args, int &_numArgs, const char *&_type, const char *&_name, const char *&_file, int &_line) \
        { \
            static ParameterArg<type> args[] = {default, ##__VA_ARGS__}; \
            _args = args; \
            _numArgs = sizeof(args) / sizeof(*args); \
            _type = #type; \
            _name = #name; \
            _file = __FILE__; \
            _line = __LINE__; \
        } \
    } name;

/////////////////////////////////////////////////////////////////
//
// Specific parameter types
//
/////////////////////////////////////////////////////////////////
#define IntParameter(name, default, ...)     Parameter(int, name, (int) default, ##__VA_ARGS__)
#define UintParameter(name, default, ...)    Parameter(unsigned, name, (unsigned) default, ##__VA_ARGS__)
#define Int64Parameter(name, default, ...)   Parameter(int64, name, (int64) default, ##__VA_ARGS__)
#define Uint64Parameter(name, default, ...)  Parameter(uint64, name, (uint64) default, ##__VA_ARGS__)
#define FloatParameter(name, default, ...)   Parameter(float, name, (float) default, ##__VA_ARGS__)
#define DoubleParameter(name, default, ...)  Parameter(double, name, (double) default, ##__VA_ARGS__)
#define StringParameter(name, default, ...)  Parameter(string, name, default, ##__VA_ARGS__)
#define BoolParameter(name, default, ...)    Parameter(bool, name, default, ##__VA_ARGS__)

/////////////////////////////////////////////////////////////////
//
// EnumParameter declaration
//
/////////////////////////////////////////////////////////////////
#define EnumParameter(name, default, description, ...) \
    enum name##_enum { __VA_ARGS__}; \
    struct E##name \
    { \
        typedef name##_enum enum_t; \
        DECLARE_ENUMERATION_BODY(E##name, __VA_ARGS__); \
    }; \
    Parameter(E##name, name, default, description, __VA_ARGS__)

/////////////////////////////////////////////////////////////////
//
// Parameter groups
//
/////////////////////////////////////////////////////////////////

struct ParameterGroupHelper
{
    ParameterGroupHelper (const char *name, const char *alt_name = NULL);
};

#define ParameterGroup(type, name, ...) \
    struct ParameterGroup__##name : public type \
    { \
        ParameterGroup__##name (ParameterGroupHelper = ParameterGroupHelper(#name, ##__VA_ARGS__)) \
        { \
            Parameter::parameterGroup(true); \
        } \
    } name

#define DeclareParameterGroup(name) \
    ParameterGroup__##name name;

#define SingletonParameterGroup(type, name, ...) \
    struct ParameterGroup__##name : public type \
    { \
        ParameterGroup__##name (ParameterGroupHelper = ParameterGroupHelper(#name, ##__VA_ARGS__)) \
        { \
            Parameter::parameterGroup(true); \
        } \
    }; \
    static descore::Singleton<ParameterGroup__##name> name;    

////////////////////////////////////////////////////////////////////////////////
//
// Parameter group array
//
////////////////////////////////////////////////////////////////////////////////

// Helper interface used to dynamically grow parameter group vectors by one
struct IParameterGroupVector
{
    virtual bool validateIndex (int index, bool logerr) = 0;
    virtual void reset () = 0;
    void registerGroupVector (const string &name);

    string m_name;
};

// Parameter group vector implementation, templated on parameter group type and initial size.
// size = 0 indicates a dynamic vector.
template <typename T>
class ParameterGroupVector : public IParameterGroupVector
{
public:
    ParameterGroupVector (const char *name, ParameterArg<int> arg1 = "", ParameterArg<int> arg2 = "")
    {
        const char *groupName = name;
        int size = 0;

        bool singleArg = true;
        if (arg1.sz)
        {
            if (*arg1.sz)
            {
                groupName = arg1.sz;
                if (!arg2.sz)
                {
                    singleArg = false;
                    size = arg2.value;
                    assert(size > 0, "Invalid parameter group array size");
                }
            }
        }
        else
        {
            size = arg1.value;
            assert(size > 0, "Invalid parameter group array size");
        }
        if (singleArg)
            assert(arg2.sz && !*arg2.sz, "Invalid parameter group array arguments");

        m_name = Parameter::parameterGroup() + groupName;
        m_size = size;
        m_dynamic = (size == 0);
        for (int i = 0 ; i < (m_dynamic ? 1 : size) ; i++)
        {
            Parameter::parameterGroup(true, str("%s[%d]", groupName, i));
            m_groups.push_back(new T);
            Parameter::parameterGroup(true);
        }
        registerGroupVector(m_name);
    }

    bool validateIndex (int index, bool logerrors)
    {
        int maxIndex = m_size - 1 + (m_dynamic ? 1 : 0);
        if (index > maxIndex)
        {
            if (logerrors)
                logerr("Index %d out of range for %s[] (must be <= %d)\n", index, *m_name, maxIndex);
            return false;
        }
        if (m_dynamic && index == maxIndex)
            push_back();
        return true;
    }

    void reset ()
    {
        if (m_dynamic)
            m_size = 0;
    }

    void push_back ()
    {
        assert(m_dynamic, "push_back() is invalid for static array %s[%d]", *m_name, m_size);
        if (++m_size > (int) m_groups.size())
        {
            assert(Parameter::parameterGroup() == "");
            Parameter::parameterGroup(true, str("%s[%d]", *m_name, m_size - 1));
            m_groups.push_back(new T);
            Parameter::parameterGroup(true);
        }
    }

    int size () const
    {
        return m_size;
    }

    T &operator[] (int index)
    {
        assert(index < m_size, "Index %d out of range for %s[%d]", index, *m_name, m_size);
        return *m_groups[index];
    }

    const T &operator[] (int index) const
    {
        assert(index < m_size, "Index %d out of range for %s[%d]", index, *m_name, m_size);
        return *m_groups[index];
    }

private:
    std::vector<T *> m_groups;  // Array of groups
    int              m_size;    // Logical size of array
    bool             m_dynamic; // True if array can be dynamically grown, false for a statically-sized array
};

// Macro for declaring a parameter group array
#define ParameterGroupArray(type, name, ...) \
    struct ParameterGroupArray__##name : public ParameterGroupVector<type> \
    { \
        ParameterGroupArray__##name () : ParameterGroupVector<type>(#name, ##__VA_ARGS__) {} \
    } name


#endif // #ifndef descore_Parameter_hpp_
