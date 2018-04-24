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

#include <storage/storage.hpp>
#include "storage_base.hpp"

#include <chrono>
#include <iostream>
#include <fstream>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>

using namespace bzn;

namespace
{
    // todo: replace with protobuf definition...
}


std::string
storage::error_msg(storage_base::result error_id)
{
    std::string message{""};
    switch(error_id)
    {
        case storage_base::result::ok:
            message = "ok";
            break;
        case storage_base::result::not_saved:
            message = "not saved";
            break;
        case storage_base::result::not_found:
            message = "not found";
            break;
        case storage_base::result::exists:
            message = "exists";
            break;
        default:
            break;
    }
    return message;
}


template<class Archive>
void
storage::serialize(Archive& ar, const uint16_t /*version*/)
{
    ar & this->kv_store;
}


bzn::uuid_t
storage::generate_random_uuid()
{
    boost::uuids::basic_random_generator<boost::mt19937> gen;
    return boost::lexical_cast<bzn::uuid_t>(gen());
}


storage_base::result
storage::create(const bzn::uuid_t& uuid, const std::string& key, const std::string& value)
{
    auto search = this->kv_store.find(uuid);

    if(search != this->kv_store.end())
    {
        if(search->second.find(key)!= search->second.end() )
        {
            return storage_base::result::exists;
        }
    }

    auto& inner_db =  this->kv_store[uuid];

    if(inner_db.find(key) == inner_db.end())
    {
        auto record = std::make_shared<bzn::storage_base::record>(bzn::storage_base::record{
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()),
            value,
            this->generate_random_uuid()});

        // todo: test if insert failed?
        inner_db.insert(std::make_pair(key,std::move(record)));
    }
    else
    {
        return storage_base::result::exists;
    }
    return storage_base::result::ok;
}


// Note: Returns nullptr if the key does not exist.
std::shared_ptr<bzn::storage_base::record>
storage::read(const bzn::uuid_t& uuid, const std::string& key)
{
    auto search = this->kv_store.find(uuid);
    if(search == this->kv_store.end())
    {
        return nullptr;
    }

    // we have the db, let's see if the key exists
    auto& inner_db = search->second;
    auto inner_search = inner_db.find(key);
    if(inner_search == inner_db.end())
    {
        return nullptr;
    }
    return inner_search->second;
}


storage_base::result
storage::update(const bzn::uuid_t& uuid, const std::string& key, const std::string& value)
{
    auto search = this->kv_store.find(uuid);
    if(search == kv_store.end())
    {
        return bzn::storage_base::result::not_found;
    }


    // we have the db, let's see if the key exists
    auto& inner_db = search->second;
    auto inner_search = inner_db.find(key);
    if(inner_search == inner_db.end())
    {
        return bzn::storage_base::result::not_found;
    }

    inner_search->second->timestamp = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch());
    inner_search->second->transaction_id = this->generate_random_uuid();
    inner_search->second->value = value;
    return storage_base::result::ok;
}


storage_base::result
storage::remove(const bzn::uuid_t& uuid, const std::string& key)
{
    auto search = this->kv_store.find(uuid);

    if(search == this->kv_store.end())
    {
        return storage_base::result::not_found;
    }

    auto record = search->second.find(key);
    if(record == search->second.end())
    {
        return storage_base::result::not_found;
    }

    search->second.erase(record);
    return storage_base::result::ok;
}


storage_base::result
storage::save(const std::string& path)
{
    try
    {
        std::ofstream ofs(path);
        boost::archive::text_oarchive oa(ofs);
        oa << this->kv_store;
    }
    catch (...)
    {
        return storage_base::result::not_saved;
    }
    return storage_base::result::ok;
}


storage_base::result
storage::load(const std::string& path)
{
    if(!boost::filesystem::exists(path))
    {
        return storage_base::result::not_found;
    }
    std::ifstream ifs(path);
    boost::archive::text_iarchive ia(ifs);
    ia >> this->kv_store;
    return storage_base::result::ok;
}


std::vector<std::string>
storage::get_keys(const bzn::uuid_t& uuid)
{
    std::vector<std::string> keys;
    auto inner_db = this->kv_store.find(uuid);

    if(inner_db == this->kv_store.end())
    {
        return keys;
    }

    for(const auto& p : inner_db->second)
    {
        keys.emplace_back(p.first);
    }
    return keys;
}