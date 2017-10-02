////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "ReplicationApplier.h"
#include "Basics/Exceptions.h"
#include "Basics/ReadLocker.h"
#include "Basics/Thread.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/files.h"
#include "Cluster/ServerState.h"
#include "Logger/Logger.h"
#include "Replication/TailingSyncer.h"
#include "Rest/Version.h"
#include "RestServer/ServerIdFeature.h"
#include "VocBase/replication-common.h"

using namespace arangodb;

/// @brief applier thread class
class ApplyThread : public Thread {
 public:
  explicit ApplyThread(std::unique_ptr<TailingSyncer>&& syncer)
      : Thread("ReplicationApplier"), _syncer(std::move(syncer)) {}

  ~ApplyThread() {
    Thread::shutdown(); 
  }

 public:
  void run() {
    TRI_ASSERT(_syncer);

    try {
      int res = _syncer->run();
      if (res != TRI_ERROR_NO_ERROR) {
        LOG_TOPIC(ERR, Logger::REPLICATION) << "Error while running applyer: "
          << TRI_errno_string(res);
      }
    } catch (std::exception const& ex) {
      LOG_TOPIC(WARN, Logger::REPLICATION) << "caught exception in ApplyThread: " << ex.what();
    } catch (...) {
      LOG_TOPIC(WARN, Logger::REPLICATION) << "caught unknown exception in ApplyThread";
    }
  }

 private:
  std::unique_ptr<TailingSyncer> _syncer;
};


ReplicationApplier::ReplicationApplier(ReplicationApplierConfiguration const& configuration,
                                       std::string&& databaseName) 
      : _configuration(configuration),
        _terminateThread(false),
        _databaseName(std::move(databaseName)) {
    setProgress(std::string("applier initially created for ") + _databaseName);
}

ReplicationApplier::~ReplicationApplier() {}

/// @brief test if the replication applier is running
bool ReplicationApplier::isRunning() const {
  READ_LOCKER(readLocker, _statusLock);
  return _state._active;
}

/// @brief block the replication applier from starting
int ReplicationApplier::preventStart() {
  WRITE_LOCKER(writeLocker, _statusLock);

  if (_state._active) {
    // already running
    return TRI_ERROR_REPLICATION_RUNNING;
  }

  if (_state._preventStart) {
    // someone else requested start prevention
    return TRI_ERROR_LOCKED;
  }

  _state._stopInitialSynchronization = false;
  _state._preventStart = true;

  return TRI_ERROR_NO_ERROR;
}

/// @brief unblock the replication applier from starting
int ReplicationApplier::allowStart() {
  WRITE_LOCKER(writeLocker, _statusLock);

  if (!_state._preventStart) {
    return TRI_ERROR_INTERNAL;
  }

  _state._stopInitialSynchronization = false;
  _state._preventStart = false;

  return TRI_ERROR_NO_ERROR;
}

/// @brief whether or not autostart option was set
bool ReplicationApplier::autoStart() const {
  READ_LOCKER(readLocker, _statusLock);
  return _configuration._autoStart;
}

/// @brief check whether the initial synchronization should be stopped
bool ReplicationApplier::stopInitialSynchronization() const {
  READ_LOCKER(readLocker, _statusLock);

  return _state._stopInitialSynchronization;
}

/// @brief stop the initial synchronization
void ReplicationApplier::stopInitialSynchronization(bool value) {
  WRITE_LOCKER(writeLocker, _statusLock);
  _state._stopInitialSynchronization = value;
}
  
/// @brief start the replication applier
void ReplicationApplier::start(TRI_voc_tick_t initialTick, bool useTick, TRI_voc_tick_t barrierId) {
  LOG_TOPIC(DEBUG, Logger::REPLICATION)
      << "requesting replication applier start. initialTick: " << initialTick
      << ", useTick: " << useTick;

  // wait until previous applier thread is shut down
  while (!wait(10 * 1000));

  WRITE_LOCKER(writeLocker, _statusLock);
  
  if (_state._preventStart) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_LOCKED);
  }

  if (_state._active) {
    // already started
    return;
  }
  
  if (_configuration._endpoint.empty() || _configuration._database.empty()) {
    setErrorNoLock(TRI_ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION, "no endpoint configured");
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION, "no endpoint configured");
  }

  // reset error
  _state._lastError.reset();

  setTermination(false);
  _state._active = true;
 
  _thread.reset(new ApplyThread(buildSyncer(initialTick, useTick, barrierId)));
  
  if (!_thread->start()) {
    _thread.reset();
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "could not start ApplyThread");
  }

  while (!_thread->hasStarted()) {
    usleep(20000);
  }

  if (useTick) {
    LOG_TOPIC(INFO, Logger::REPLICATION)
        << "started replication applier for " << _databaseName
        << ", endpoint '" << _configuration._endpoint << "' from tick "
        << initialTick;
  } else {
    LOG_TOPIC(INFO, Logger::REPLICATION)
        << "re-started replication applier for "
        << _databaseName << ", endpoint '" << _configuration._endpoint
        << "'";
  }
}

/// @brief stop the replication applier
void ReplicationApplier::stop(bool resetError, bool joinThread) {
  {
    WRITE_LOCKER(writeLocker, _statusLock);

    // always stop initial synchronization
    _state._stopInitialSynchronization = true;

    if (!_state._active) {
      // not active
      return;
    }

    _state._active = false;

    setTermination(true);
    setProgressNoLock("applier shut down");

    if (resetError) {
      _state.clearError();
    }
  }

  // join the thread without holding the status lock 
  // (otherwise it would probably not join)
  if (joinThread) {
    TRI_ASSERT(_thread);
    _thread.reset();
    setTermination(false);
  }

  LOG_TOPIC(INFO, Logger::REPLICATION)
      << "stopped replication applier for database '" << _databaseName << "'";
}

/// @brief shuts down the replication applier
void ReplicationApplier::shutdown() {
  {
    WRITE_LOCKER(writeLocker, _statusLock);

    if (!_state._active) {
      // nothing to do
      return;
    }

    _state._active = false;
    _state.clearError();

    setTermination(true);
    setProgressNoLock("applier stopped");
  }

  // join the thread without holding the status lock 
  // (otherwise it would probably not join)
  TRI_ASSERT(_thread);
  _thread.reset();

  setTermination(false);

  LOG_TOPIC(INFO, Logger::REPLICATION)
      << "shut down replication applier for " << _databaseName;
}

void ReplicationApplier::removeState() {
  WRITE_LOCKER(writeLocker, _statusLock);
  _state.reset();

  std::string const filename = getStateFilename();

  if (TRI_ExistsFile(filename.c_str())) {
    LOG_TOPIC(TRACE, Logger::REPLICATION) << "removing replication state file '"
                                          << filename << "'";
    int res = TRI_UnlinkFile(filename.c_str());

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION_MESSAGE(res, std::string("unable to remove replication state file '") + filename + "'");
    }
  }
} 
  
void ReplicationApplier::reconfigure(ReplicationApplierConfiguration const& configuration) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  
  if (configuration._endpoint.empty()) {
    // no endpoint
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION, "no endpoint configured");
  }

  WRITE_LOCKER(writeLocker, _statusLock);

  if (_state._active) {
    // cannot change the configuration while the replication is still running
    THROW_ARANGO_EXCEPTION(TRI_ERROR_REPLICATION_RUNNING);
  }

  _configuration = configuration;
  storeConfiguration(true);
}

/// @brief store the applier state in persistent storage
/// must currently be called while holding the write-lock
void ReplicationApplier::persistState(bool doSync) {
  VPackBuilder builder;
  _state.toVelocyPack(builder, false);

  std::string const filename = getStateFilename();
  LOG_TOPIC(TRACE, Logger::REPLICATION)
      << "saving replication applier state to file '" << filename << "'";

  if (!basics::VelocyPackHelper::velocyPackToFile(filename, builder.slice(), doSync)) {
    THROW_ARANGO_EXCEPTION(TRI_errno());
  }
}
  
/// @brief store the current applier state in the passed vpack builder 
void ReplicationApplier::toVelocyPack(arangodb::velocypack::Builder& result) const {
  TRI_ASSERT(!result.isClosed());

  ReplicationApplierConfiguration configuration;
  ReplicationApplierState state;

  {
    // copy current config and state under the lock
    READ_LOCKER(readLocker, _statusLock);
    configuration = _configuration;
    state = _state;
  }

  // add state
  result.add(VPackValue("state"));
  state.toVelocyPack(result, true);

  // add server info
  result.add("server", VPackValue(VPackValueType::Object));
  result.add("version", VPackValue(ARANGODB_VERSION));
  result.add("serverId", VPackValue(std::to_string(ServerIdFeature::getId())));
  result.close();  // server
  
  if (!configuration._endpoint.empty()) {
    result.add("endpoint", VPackValue(configuration._endpoint));
  }
  if (!configuration._database.empty()) {
    result.add("database", VPackValue(configuration._database));
  }
}
  
/// @brief return the current configuration
ReplicationApplierConfiguration ReplicationApplier::configuration() const {
  READ_LOCKER(readLocker, _statusLock);
  return _configuration;
}

/// @brief register an applier error
int ReplicationApplier::setError(int errorCode, char const* msg) {
  return setErrorNoLock(errorCode, std::string(msg));
}

int ReplicationApplier::setError(int errorCode, std::string const& msg) {
  WRITE_LOCKER(writeLocker, _statusLock);
  return setErrorNoLock(errorCode, msg);
}

/// @brief set the progress 
void ReplicationApplier::setProgress(char const* msg) {
  return setProgress(std::string(msg));
}

void ReplicationApplier::setProgress(std::string const& msg) {
  WRITE_LOCKER(writeLocker, _statusLock);
  setProgressNoLock(msg);
}

/// @brief register an applier error
int ReplicationApplier::setErrorNoLock(int errorCode, std::string const& msg) {
  // log error message
  if (errorCode != TRI_ERROR_REPLICATION_APPLIER_STOPPED) {
    LOG_TOPIC(ERR, Logger::REPLICATION)
        << "replication applier error for " << _databaseName << ": "
        << (msg.empty() ? TRI_errno_string(errorCode) : msg);
  }

  _state.setError(errorCode, msg.empty() ? TRI_errno_string(errorCode) : msg);
  return errorCode;
}

void ReplicationApplier::setProgressNoLock(std::string const& msg) {
  _state._progressMsg = msg;

  // write time into buffer
  TRI_GetTimeStampReplication(_state._progressTime, sizeof(_state._progressTime) - 1);
}

/// @brief pauses and checks whether the apply thread should terminate
bool ReplicationApplier::wait(uint64_t sleepTime) {
  if (isTerminated()) {
    return false;
  }
  
  if (sleepTime > 0) {
    LOG_TOPIC(TRACE, Logger::REPLICATION)
    << "replication applier going to sleep for " << sleepTime << " ns";
    
    static uint64_t const SleepChunk = 500 * 1000;
    
    while (sleepTime >= SleepChunk) {
      usleep(static_cast<TRI_usleep_t>(SleepChunk));
      sleepTime -= SleepChunk;
      
      if (isTerminated()) {
        return false;
      }
    }
    
    if (sleepTime > 0) {
      usleep(static_cast<TRI_usleep_t>(sleepTime));
      
      if (isTerminated()) {
        return false;
      }
    }
  }
  
  return true;
}
