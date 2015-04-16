// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "misc_log_ex.h"

#include "cryptonote_config.h"
#include "checkpoints.h"

#define ADD_CHECKPOINT(h, hash)  CHECK_AND_ASSERT(checkpoints.add_checkpoint(h, hash), false);

namespace cryptonote {
  inline bool create_checkpoints(cryptonote::checkpoints& checkpoints)
  {
    if (cryptonote::config::testnet)
    {
      ADD_CHECKPOINT(639, "2378f9e81768e08df767ca4257d408a8925c6c6203cdb911cbb1f3b8a6a35c47");
    }
    else
    {
      ADD_CHECKPOINT(79000, "a0f97e100a8b48741a841659b14ecbadd80dc5b8b41fbb58f9202c03c190d8f1");
    }

    return true;
  }
}
