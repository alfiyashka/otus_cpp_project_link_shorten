#include "TokenGenerator.hpp"
#include <string>

#include <sqids/sqids.hpp>
#include "IdGenerator.hpp"
#include  <cstring>

std::string TokenGenerator::generateToken()
{
    sqidscxx::Sqids<int64_t> sqids({ minLength: 15 });
    const auto id = IDGenerator::getGenerator()->generateId();
    std::vector<int64_t> encodedToInt({id});
    const auto idEncoded = sqids.encode(encodedToInt);
    
    std::cout << "Generated id " << idEncoded << std::endl;
    return idEncoded;
}
