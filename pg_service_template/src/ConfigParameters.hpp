#pragma once

#include <string>

#include <boost/unordered_map.hpp>
#include <boost/assign/list_of.hpp>


enum ConfigParametersEnum {
  request_wait_timeout,
  request_try_attempt,
  clean_db_period,
  expired_token_timestamp
};

const boost::unordered_map<ConfigParametersEnum,std::string> ConfigParametersMap = boost::assign::map_list_of
    (request_wait_timeout, "request_wait_timeout")
    (request_try_attempt,"request_try_attempt")
    (expired_token_timestamp, "expired_token_timestamp")
    (clean_db_period, "clean_db_period");
;

