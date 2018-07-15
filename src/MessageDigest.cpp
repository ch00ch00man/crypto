// Copyright 2011 Boris Kogan (boris@thekogans.net)
//
// This file is part of libthekogans_crypto.
//
// libthekogans_crypto is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// libthekogans_crypto is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with libthekogans_crypto. If not, see <http://www.gnu.org/licenses/>.

#include <cstring>
#include <openssl/evp.h>
#include "thekogans/util/File.h"
#include "thekogans/util/FixedArray.h"
#include "thekogans/crypto/OpenSSLInit.h"
#include "thekogans/crypto/OpenSSLException.h"
#include "thekogans/crypto/MessageDigest.h"

namespace thekogans {
    namespace crypto {

        MessageDigest::MessageDigest (const EVP_MD *md_) :
                md (md_) {
            if (md != 0) {
                if (EVP_DigestInit_ex (&ctx, md, OpenSSLInit::engine) != 1) {
                    THEKOGANS_CRYPTO_THROW_OPENSSL_EXCEPTION;
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void MessageDigest::Init () {
            if (EVP_DigestInit_ex (&ctx, 0, 0) != 1) {
                THEKOGANS_CRYPTO_THROW_OPENSSL_EXCEPTION;
            }
        }

        void MessageDigest::Update (
                const void *buffer,
                std::size_t bufferLength) {
            if (buffer != 0 && bufferLength > 0) {
                if (EVP_DigestUpdate (&ctx, buffer, bufferLength) != 1) {
                    THEKOGANS_CRYPTO_THROW_OPENSSL_EXCEPTION;
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        std::size_t MessageDigest::Final (util::ui8 *digest) {
            if (digest != 0) {
                util::ui32 digestLength = 0;
                if (EVP_DigestFinal_ex (&ctx, digest, &digestLength) == 1) {
                    return digestLength;
                }
                else {
                    THEKOGANS_CRYPTO_THROW_OPENSSL_EXCEPTION;
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        util::Buffer MessageDigest::HashBuffer (
                const void *buffer,
                std::size_t bufferLength) {
            if (buffer != 0 && bufferLength > 0) {
                Init ();
                Update (buffer, bufferLength);
                util::Buffer hash (util::HostEndian, GetMDLength (md));
                hash.AdvanceWriteOffset (Final (hash.GetWritePtr ()));
                assert (hash.GetDataAvailableForWriting () == 0);
                return hash;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        util::Buffer MessageDigest::HashFile (const std::string &path) {
            util::ReadOnlyFile file (util::HostEndian, path);
            Init ();
            util::FixedArray<util::ui8, 4096> buffer;
            for (std::size_t count = file.Read (buffer, 4096);
                    count != 0;
                    count = file.Read (buffer, 4096)) {
                Update (buffer, count);
            }
            util::Buffer hash (util::HostEndian, GetMDLength (md));
            hash.AdvanceWriteOffset (Final (hash.GetWritePtr ()));
            assert (hash.GetDataAvailableForWriting () == 0);
            return hash;
        }

    } // namespace crypto
} // namespace thekogans
