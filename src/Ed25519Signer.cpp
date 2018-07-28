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

#include <vector>
#include "thekogans/util/Types.h"
#include "thekogans/crypto/Curve25519.h"
#include "thekogans/crypto/Ed25519AsymmetricKey.h"
#include "thekogans/crypto/Ed25519Signer.h"

namespace thekogans {
    namespace crypto {

        THEKOGANS_CRYPTO_IMPLEMENT_SIGNER (Ed25519Signer, Ed25519AsymmetricKey::KEY_TYPE)

        Ed25519Signer::Ed25519Signer (
                AsymmetricKey::Ptr privateKey,
                MessageDigest::Ptr messageDigest) :
                Signer (privateKey, messageDigest) {
            if (privateKey.Get () == 0 || !privateKey->IsPrivate () ||
                    privateKey->GetKeyType () != Ed25519AsymmetricKey::KEY_TYPE) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void Ed25519Signer::Init () {
            messageDigest->Init ();
        }

        void Ed25519Signer::Update (
                const void *buffer,
                std::size_t bufferLength) {
            messageDigest->Update (buffer, bufferLength);
        }

        std::size_t Ed25519Signer::Final (util::ui8 *signature) {
            std::vector<util::ui8> digest (messageDigest->GetDigestLength ());
            messageDigest->Final (digest.data ());
            return Ed25519::SignBuffer (
                digest.data (),
                digest.size (),
                (const util::ui8 *)privateKey->GetKey (),
                signature);
        }

    } // namespace crypto
} // namespace thekogans
