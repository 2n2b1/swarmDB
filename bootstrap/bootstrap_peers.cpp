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

#include <utils/blacklist.hpp>
#include <utils/crypto.hpp>
#include <utils/http_get.hpp>
#include <utils/ESR_Peers_List.h>
#include <bootstrap/bootstrap_peers.hpp>
#include <fstream>
#include <sstream>
#include <json/json.h>
#include <algorithm>
#include <iostream>

using namespace bzn;


bool
bootstrap_peers::fetch_peers_from_file(const std::string& filename)
{
    std::ifstream file(filename);
    if (file.fail())
    {
        LOG(error) << "Failed to read bootstrap peers file " << filename;
        return false;
    }

    LOG(info) << "Reading peers from " << filename;

    return ingest_json(file);
}


bool 
bootstrap_peers::fetch_peers_from_url(const std::string& url)
{
    std::string peers = bzn::utils::http::sync_get(url);

    LOG(info) << "Downloaded peer list from " << url;

    std::stringstream stream;
    stream << peers;

    return ingest_json(stream);
}


bool
bootstrap_peers::fetch_peers_from_contract(const bzn::uuid_t& swarm_id)
{
    auto peer_urls = bzn::utils::ESR::get_peer_urls(swarm_id);
    bzn::json_message peers;
    for (const auto& url : peer_urls)
    {
        bzn::peer_address_t peer_info{bzn::utils::ESR::get_peer_info(swarm_id, url)};
        Json::Value json_url;
        json_url["host"] = peer_info.name;
        json_url["http_port"] = peer_info.http_port;
        json_url["name"] = peer_info.name;
        json_url["port"] = peer_info.port;
        json_url["uuid"] = peer_info.uuid;
        peers.append(json_url);
    }

    std::stringstream stream;
    stream << peers;

    return ingest_json(stream);
}


const bzn::peers_list_t&
bootstrap_peers::get_peers() const
{
    return this->peer_addresses;
}


size_t
bootstrap_peers::initialize_peer_list(const Json::Value& root, bzn::peers_list_t& /*peer_addresses*/)
{
    size_t valid_addresses_read = 0;
    // Expect the read json to be an array of peer objects
    for (const auto& peer : root)
    {
        std::string host;
        std::string name;
        std::string uuid;
        uint16_t    port;
        uint16_t    http_port;

        try
        {
            host = peer["host"].asString();
            port = peer["port"].asUInt();
            http_port = peer["http_port"].asUInt();
            uuid = peer.isMember("uuid") ? peer["uuid"].asString() : "unknown";
            name = peer.isMember("name") ? peer["name"].asString() : "unknown";
        }
        catch(std::exception& e)
        {
            LOG(warning) << "Ignoring malformed peer specification " << peer;
            continue;
        }

        // port wasn't actually a 16 bit uint
        if (peer["port"].asUInt() != port)
        {
            LOG(warning) << "Ignoring peer with bad port " << peer;
            continue;
        }

        if (peer["http_port"].asUInt() != http_port)
        {
            LOG(warning) << "Ignoring peer with bad http port " << peer;
            continue;
        }

        // peer didn't contain everything we need
        if (host.empty() || port == 0 || http_port == 0)
        {
            LOG(warning) << "Ignoring underspecified peer (needs host, port and http_port) " << peer;
            continue;
        }

        if (this->is_peer_validation_enabled())
        {
            // At this point we cannot validate uuids as we do not have
            // signatures for all of them, so we simply ignore blacklisted
            // uuids
            if (bzn::utils::blacklist::is_blacklisted(uuid))
            {
                LOG(warning) << "Ignoring blacklisted node with uuid: [" << uuid << "]";
                continue;
            }
        }

        this->peer_addresses.emplace(host, port, http_port, name, uuid);

        LOG(trace) << "Found peer " << host << ":" << port << " (" << name << ")";

        valid_addresses_read++;
    }
    return valid_addresses_read;
}


bool
bootstrap_peers::ingest_json(std::istream& peers)
{
    size_t addresses_before = this->peer_addresses.size();
    Json::Value root;

    try
    {
        peers >> root;
    }
    catch (const std::exception& e)
    {
        LOG(error) << "Failed to parse peer JSON (" << e.what() << ")";
        return false;
    }

    size_t valid_addresses_read = this->initialize_peer_list(root, this->peer_addresses);

    size_t new_addresses = this->peer_addresses.size() - addresses_before;
    size_t duplicate_addresses = new_addresses - valid_addresses_read;

    LOG(info) << "Found " << new_addresses << " new peers";

    if (duplicate_addresses > 0)
    {
        LOG(info) << "Ignored " << duplicate_addresses << " duplicate addresses";
    }

    return new_addresses > 0;
}
