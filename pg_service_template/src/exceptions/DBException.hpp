#ifndef __DB_EXCEPTION_HPP_
#define __DB_EXCEPTION_HPP_

#include <stdexcept>
#include <string>

class DBException : public std::runtime_error
{
private:
    const std::string m_reason;
public:
    DBException(const char* reason): 
       std::runtime_error((std::string("Encountered following error from postgres db : ") + reason).c_str()),
       m_reason(reason)
    {

    }
    const std::string& reason () const { return m_reason; }
    

};


#endif