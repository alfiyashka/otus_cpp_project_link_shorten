#ifndef __INTERNAL_EXCEPTION_HPP_
#define __INTERNAL_EXCEPTION_HPP_

#include <stdexcept>
#include <string>

class InternalLogicException : public std::runtime_error
{
private:
    const std::string m_reason;
public:
    InternalLogicException(const char* reason): 
       std::runtime_error((std::string("Encountered following internal logic error : ") + reason).c_str()),
       m_reason(reason)
    {

    }
    const std::string& reason () const { return m_reason; }    

};


#endif