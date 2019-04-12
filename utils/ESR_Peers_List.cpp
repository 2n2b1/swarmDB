// Copyright (C) 2018 Bluzelle
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License, version 3,
// as published by the Free Software Foundation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

// The following curl will get a list of peers for a given swarm id:
// curl --data '{"jsonrpc":"2.0","method":"eth_call","params":[{"to":"0x3e4BDd757322eF358664090c69b6927BdB6cf2De","data": "0x46e76d8b0000000000000000000000000000000000000000000000000000000000000020000000000000000000000000000000000000000000000000000000000000000d426c757a656c6c65537761726d00000000000000000000000000000000000000" },"latest"],"id":1}' https://ropsten.infura.io/uvek7IebbbHoP8Bb9NkV

// Given a swarm id and a node address, the following curl will get the node info
// curl --data '{"jsonrpc":"2.0","method":"eth_call","params":[{"to":"0x3e4BDd757322eF358664090c69b6927BdB6cf2De","data": "0xcc8575cb00000000000000000000000000000000000000000000000000000000000000400000000000000000000000000000000000000000000000000000000000000080000000000000000000000000000000000000000000000000000000000000000d426c757a656c6c65537761726d00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000c3230392e35302e36312e38370000000000000000000000000000000000000000" },"latest"],"id":1}' https://ropsten.infura.io/uvek7IebbbHoP8Bb9NkV

#include <utils/curl.hpp>
#include <utils/ESR_Peers_List.h>
#include <boost/algorithm/hex.hpp>
#include <json/json.h>
#include <iostream>
#include <sstream>

namespace
{
    const std::string ERR_UNABLE_TO_PARSE_JSON_RESPONSE{"Unable to parse JSON response: "};
    const std::string CONTRACT_ADDRESS{"3e4BDd757322eF358664090c69b6927BdB6cf2De"}; // 407cA38Ded7929843BCF1f00A3303FC7dFdc12fC
    const std::string GET_PEERS_ADDRESS{"46e76d8b"};//46e76d8b
    const std::string GET_PEER_INFO_ADDRESS{"cc8575cb"};

    bzn::json_message
    parse_curl_response_to_json(const std::string& response)
    {
        bzn::json_message json_msg;
        Json::CharReaderBuilder builder;
        Json::CharReader* reader = builder.newCharReader();
        std::string errors;
        if(!reader->parse(
                response.c_str()
                , response.c_str() + response.size()
                , &json_msg
                , &errors))
        {
            throw (std::runtime_error(ERR_UNABLE_TO_PARSE_JSON_RESPONSE + errors));
        }
        return json_msg;
    }


    bzn::json_message
    make_params(const std::string& to_hex, const std::string& data_hex)
    {
        bzn::json_message params;
        bzn::json_message param;
        param["to"] = "0x" + to_hex; //407cA38Ded7929843BCF1f00A3303FC7dFdc12fC";
        param["data"] = "0x" + data_hex; // 46e76d8b0000000000000000000000000000000000000000000000000000000000000020000000000000000000000000000000000000000000000000000000000000000d426c757a656c6c65537761726d00000000000000000000000000000000000000";
        params.append(param);
        params.append("latest");
        return params;
    }


    std::string
    make_request(const std::string& to_hex, const std::string& data_hex)
    {
        bzn::json_message request;
        request["jsonrpc"] = "2.0";
        request["method"] = "eth_call";
        request["params"] = make_params(to_hex, data_hex);
        request["id"] = 1;
        Json::StreamWriterBuilder wbuilder;
        wbuilder["indentation"] = "";
        return Json::writeString(wbuilder, request);
    }


    std::string
    hex_to_utf8(const std::string& hex)
    {
        std::stringstream utft8stream;
        boost::algorithm::unhex(hex, std::ostream_iterator<char>{utft8stream, ""});
        return utft8stream.str();
    }


    std::string
    string_to_hex(const std::string& value)
    {
        std::stringstream hexstream;
        boost::algorithm::hex(value, std::ostream_iterator<char>{hexstream, ""});
        return hexstream.str();
    }


    std::string
    filter_string(const std::string& in)
    {
        std::string out;
        std::copy_if(in.begin(), in.end(), std::back_inserter(out), [](const auto& c) { return isalnum(c) || c == '.'; });
        return out;
    }


    std::vector<std::string>
    solidity_result_to_vector(const std::string& result)
    {
        std::vector<std::string> results;
        std::string hex;
        for (const auto& ch : result)
        {
            hex += ch;
            if (hex.size() == 64)
            {
                std::string item{filter_string(hex_to_utf8(hex))};
                if (item.length()>0)
                {
                    results.push_back(item);
                }
                hex = "";
            }
        }
        return results;
    }


    std::string
    pad_parameter_to_64(const std::string& parameter)
    {
        std::string result{parameter};
        if (64 > result.size())
        {
            result.insert(result.size(), 64 - result.size(), '0');
        }
        return result;
    }
}


namespace bzn::utils::ESR {

    std::vector<std::string>
    get_peer_urls(const bzn::uuid_t &swarm_id, const std::string &url/* = ROPSTEN_URL*/) {
        const std::string contract_parameters{
                pad_parameter_to_64(GET_PEERS_ADDRESS) +
                "0000002000000000000000000000000000000000000000000000000000000000"
                "0000000d"
                + pad_parameter_to_64(swarm_id) +
                "00"
        };
        const std::string response{bzn::utils::curl::perform_curl_request(url.c_str(), make_request(CONTRACT_ADDRESS,
                                                                                                    contract_parameters))};
        const bzn::json_message json_response{parse_curl_response_to_json(response)};
        const std::string result{json_response["result"].asCString() + 2};
        const auto results{solidity_result_to_vector(result)};
        return results;
    }

    bzn::peer_address_t
    get_peer_info(const bzn::uuid_t &swarm_id, const std::string &node_url, const std::string &url/* = ROPSTEN_URL*/) {
        const std::string contract_parameters{
                pad_parameter_to_64(GET_PEER_INFO_ADDRESS) +
                "0000004000000000000000000000000000000000000000000000000000000000"
                "00000080000000000000000000000000000000000000000000000000000000000000000d"
                + pad_parameter_to_64(swarm_id) +
                "000000000000000000000000000000000000000000000000000000000000000c"
                + pad_parameter_to_64(string_to_hex(node_url))
        };
        const std::string response{bzn::utils::curl::perform_curl_request(url.c_str(), make_request(CONTRACT_ADDRESS,
                                                                                                    contract_parameters))};
        const bzn::json_message json_response{parse_curl_response_to_json(response)};
        const std::string result{json_response["result"].asCString() + 2};
        const auto results{solidity_result_to_vector(result)};
        return results.size() > 0 ? bzn::peer_address_t(results[0].c_str(), 0, 0, results[1].c_str(), "")
                                  : bzn::peer_address_t("", 0, 0, "", "");
    }
}
