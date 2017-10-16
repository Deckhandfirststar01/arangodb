////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "RestClusterHandler.h"
#include "Agency/AgencyComm.h"
#include "Agency/Supervision.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Replication/ReplicationFeature.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include "Rest/HttpRequest.h"
#include "Rest/Version.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestClusterHandler::RestClusterHandler(GeneralRequest* request,
                                 GeneralResponse* response)
    : RestBaseHandler(request, response) {}

RestStatus RestClusterHandler::execute() {

  if (_request->requestType() != RequestType::GET) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN,
                  "only the GET method is allowed");
    return RestStatus::DONE;
  }
  
  std::vector<std::string> const& suffixes = _request->suffixes();
  if (!suffixes.empty() && suffixes[0] == "endpoints") {
    handleCommandEndpoints();
  } /*else if (!suffixes.empty() && suffixes[0] == "serverInfo") {
    handleCommandServerInfo();
  }*/ else {
    generateError(Result(TRI_ERROR_FORBIDDEN,
                         "expecting _api/cluster/endpoints"));
  }

  return RestStatus::DONE;
}

// //////////////////////////////////////////////////////////////////////////////
// / @start Docu Block JSF_getClusterEndpoints
// / @brief returns information about all coordinator endpoints
// /
// / @ RESTHEADER{GET /_api/cluster/endpoints, Get information
// / about all coordinator endpoints
// /
// / @ RESTDESCRIPTION Returns an array of objects, which each have
// / the attribute `endpoint`, whose value is a string with the endpoint
// / description. There is an entry for each coordinator in the cluster.
// /
// / @ RESTRETURNCODES
// /
// / @ RESTRETURNCODE{200} is returned when everything went well.
// /
// / @ RESTRETURNCODE{403} server is not a coordinator or method was not GET.
// /
// / @end Docu Block
// //////////////////////////////////////////////////////////////////////////////
void RestClusterHandler::handleCommandEndpoints() {
  TRI_ASSERT(AgencyCommManager::isEnabled());
  ClusterInfo* ci = ClusterInfo::instance();
  TRI_ASSERT(ci != nullptr);
  std::vector<ServerID> endpoints;
  
  if (ServerState::instance()->isCoordinator()) {
    endpoints = ci->getCurrentCoordinators();
  } else if (ServerState::instance()->isSingleServer()) {
    
    ReplicationFeature* replication = ReplicationFeature::INSTANCE;
    if (!replication->isAutomaticFailoverEnabled() ||
        !AgencyCommManager::isEnabled()) {
      generateError(Result(TRI_ERROR_FORBIDDEN,
                           "automatic failover is not enabled"));
      return;
    }
    
    std::string const leaderPath = "Plan/AsyncReplication/Leader";
    std::string const healthPath = "Supervision/Health";
    AgencyComm agency;
    
    AgencyReadTransaction trx(std::vector<std::string>({
      AgencyCommManager::path(healthPath),
      AgencyCommManager::path(leaderPath)}));
    AgencyCommResult result = agency.sendTransactionWithFailover(trx, 1.0);
    
    if (!result.successful()) {
      generateError(ResponseCode::SERVER_ERROR, result.errorCode(),
                    result.errorMessage());
      return;
    }
    
    std::vector<std::string> path = AgencyCommManager::slicePath(leaderPath);
    VPackSlice slice = result.slice()[0].get(path);
    ServerID leaderId = slice.isString() ? slice.copyString() : "";
    path = AgencyCommManager::slicePath(healthPath);
    VPackSlice healthMap = result.slice()[0].get(path);
    
    
    
    if (!leaderId.empty()) {
      endpoints.push_back(leaderId);
      
      // {"serverId" : {"Status" : "GOOD", ...}}
      for (VPackObjectIterator::ObjectPair const& pair : VPackObjectIterator(healthMap)) {
        TRI_ASSERT(pair.key.isString() && pair.value.isObject());
        if (pair.key.compareString(leaderId) != 0) {
          VPackSlice status = pair.value.get("Status");
          if (status.isString() &&
              status.compareString(consensus::Supervision::HEALTH_STATUS_GOOD) == 0) {
            endpoints.push_back(pair.key.copyString());
          }
        }
      }
      
    } else {
      LOG_TOPIC(INFO, Logger::CLUSTER) << "Leadership challenge is ongoing";
    }
    
  } else {
    generateError(Result(TRI_ERROR_FORBIDDEN,
                         "only coordinators can serve this request"));
    return;
  }
  
  VPackBuilder builder;
  builder.openObject();
  builder.add("error", VPackValue(false));
  builder.add("code", VPackValue(200));
  {
    VPackArrayBuilder array(&builder, "endpoints", true);
    for (ServerID const& sid : endpoints) {
      array->add(VPackValue(ci->getServerEndpoint(sid)));
    }
  }
  builder.close();
  generateResult(rest::ResponseCode::OK, builder.slice());
}

