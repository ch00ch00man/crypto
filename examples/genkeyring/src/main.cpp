// Copyright 2011 Boris Kogan (boris@thekogans.net)
//
// This file is part of libthekogans_util.
//
// libthekogans_util is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// libthekogans_util is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with libthekogans_util. If not, see <http://www.gnu.org/licenses/>.

#include "thekogans/util/CommandLineOptions.h"
#include "thekogans/util/LoggerMgr.h"
#include "thekogans/util/ConsoleLogger.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/RandomSource.h"
#include "thekogans/util/FixedBuffer.h"
#include "thekogans/crypto/OpenSSLInit.h"
#include "thekogans/crypto/CipherSuite.h"
#include "thekogans/crypto/Cipher.h"
#include "thekogans/crypto/KeyRing.h"

using namespace thekogans;

namespace {
    std::string GetCipherSuites () {
        std::string cipherSuites_;
        const std::vector<crypto::CipherSuite> &cipherSuites =
            crypto::CipherSuite::GetCipherSuites ();
        if (!cipherSuites.empty ()) {
            cipherSuites_ = cipherSuites[0].ToString ();
            for (std::size_t i = 1, count = cipherSuites.size (); i < count; ++i) {
                cipherSuites_ += " | " + cipherSuites[i].ToString ();
            }
        }
        return cipherSuites_;
    }
}

int main (
        int argc,
        const char *argv[]) {
    struct Options : public util::CommandLineOptions {
        bool help;
        crypto::CipherSuite cipherSuite;
        std::string name;
        std::string description;
        std::string password;
        std::string path;

        Options () :
            help (false),
            cipherSuite ("ECDHE_ECDSA_AES-256-GCM_SHA2-512") {}

        virtual void DoOption (
                char option,
                const std::string &value) {
            switch (option) {
                case 'h': {
                    help = true;
                    break;
                }
                case 'c': {
                    cipherSuite = crypto::CipherSuite (value);
                    break;
                }
                case 'n': {
                    name = value;
                    break;
                }
                case 'd': {
                    description = value;
                    break;
                }
                case 'p': {
                    password = value;
                    break;
                }
            }
        }
        virtual void DoPath (const std::string &value) {
            path = value;
        }
    } options;
    options.Parse (argc, argv, "hcndp");
    if (options.help ||
            options.password.empty () ||
            options.path.empty ()) {
        std::cout << "usage: " << argv[0] << " [-h] [-c:'" << GetCipherSuites () << "'] "
            "[-n:'optional key ring name'] [-d:'optional key ring description'] "
            "-p:password path" << std::endl;
        return 1;
    }
    THEKOGANS_UTIL_LOG_INIT (
        util::LoggerMgr::Debug,
        util::LoggerMgr::All);
    THEKOGANS_UTIL_LOG_ADD_LOGGER (
        util::Logger::Ptr (new util::ConsoleLogger));
    THEKOGANS_UTIL_TRY {
        crypto::OpenSSLInit openSSLInit;
        {
            std::cout << "Generating key ring...";
            crypto::KeyRing keyRing (
                options.cipherSuite,
                options.name,
                options.description);
            crypto::Cipher cipher (
                crypto::SymmetricKey::FromSecretAndSalt (
                    crypto::Cipher::GetKeyLength (),
                    options.password.c_str (),
                    options.password.size ()));
            keyRing.Save (options.path, &cipher);
            std::cout << "Done" << std::endl << "Master Key ID: " <<
                keyRing.GetMasterKey ()->GetId ().ToString () << std::endl;
        }
        {
            std::cout << "Verifying key ring...";
            crypto::Cipher cipher (
                crypto::SymmetricKey::FromSecretAndSalt (
                    crypto::Cipher::GetKeyLength (),
                    options.password.c_str (),
                    options.password.size ()));
            crypto::KeyRing::Ptr keyRing = crypto::KeyRing::Load (options.path, &cipher);
            {
                util::FixedBuffer<256> originalPlaintext;
                originalPlaintext.AdvanceWriteOffset (
                    util::GlobalRandomSource::Instance ().GetBytes (
                        originalPlaintext.GetWritePtr (),
                        originalPlaintext.GetDataAvailableForWriting ()));
                crypto::Cipher cipher (keyRing->GetMasterKey ());
                util::Buffer::UniquePtr ciphertext = cipher.Encrypt (
                    originalPlaintext.GetReadPtr (),
                    originalPlaintext.GetDataAvailableForReading ());
                util::Buffer::UniquePtr decryptedPlaintext = cipher.Decrypt (
                    ciphertext->GetReadPtr (),
                    ciphertext->GetDataAvailableForReading ());
                if (originalPlaintext.GetDataAvailableForReading () ==
                        decryptedPlaintext->GetDataAvailableForReading () &&
                        memcmp (
                            originalPlaintext.GetReadPtr (),
                            decryptedPlaintext->GetReadPtr (),
                            originalPlaintext.GetDataAvailableForReading ()) == 0) {
                    std::cout << "Passed" << std::endl;
                }
                else {
                    std::cout << "Failed" << std::endl;
                }
            }
        }
    }
    THEKOGANS_UTIL_CATCH_AND_LOG
    THEKOGANS_UTIL_LOG_FLUSH
    return 0;
}
