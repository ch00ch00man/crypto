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

#if !defined (__thekogans_crypto_Authenticator_h)
#define __thekogans_crypto_Authenticator_h

#include <cstddef>
#include <openssl/evp.h>
#include "thekogans/util/RefCounted.h"
#include "thekogans/util/Buffer.h"
#include "thekogans/crypto/Config.h"
#include "thekogans/crypto/AsymmetricKey.h"
#include "thekogans/crypto/Signer.h"
#include "thekogans/crypto/Verifier.h"

namespace thekogans {
    namespace crypto {

        /// \struct Authenticator Authenticator.h thekogans/crypto/Authenticator.h
        ///
        /// \brief
        /// Authenticator implements public key signing and verifying operations.
        /// NOTE: You can call Sign[Buffer | File] and Verify[Buffer | File]Signature
        /// as many times as you need and in any order. Authenticator is designed to
        /// be reused. It will reset it's internal state after every sign/verify
        /// operation ready for the next.

        struct _LIB_THEKOGANS_CRYPTO_DECL Authenticator : public util::ThreadSafeRefCounted {
            /// \brief
            /// Convenient typedef for util::ThreadSafeRefCounted::Ptr<Authenticator>.
            typedef util::ThreadSafeRefCounted::Ptr<Authenticator> Ptr;

            enum Op {
                /// \brief
                /// Perform the signing operation.
                Sign,
                /// \brief
                /// Perform the verify operation.
                Verify
            };

        private:
            /// \brief
            /// Operation (Sign/Verify) to perform.
            Op op;
            /// \brief
            /// Used if op == Sign.
            Signer::Ptr signer;
            /// \brief
            /// Used if op == Verify.
            Verifier::Ptr verifier;

        public:
            /// \brief
            /// ctor.
            /// \param[in] op_ Operation (Sign/Verify) to perform.
            /// \param[in] key Private (Sign)/Public (Verify) key.
            /// \param[in] md OpenSSL message digest to use.
            Authenticator (
                Op op_,
                AsymmetricKey::Ptr key,
                const EVP_MD *md = THEKOGANS_CRYPTO_DEFAULT_MD) :
                op (op_),
                signer (op == Sign ? new Signer (key, md) : 0),
                verifier (op == Verify ? new Verifier (key, md) : 0) {}

            /// \break
            /// Return the operation (Op) this authenticator performs.
            /// \return Operation (Op) this authenticator performs.
            inline Op GetOp () const {
                return op;
            }

            /// \brief
            /// Return the key associated with this authenticator.
            /// \return \see{Signer} or \see{Verifier} key (depending on op).
            inline AsymmetricKey::Ptr GetKey () const {
                return op == Sign ? signer->GetKey () : verifier->GetKey ();
            }

            /// \brief
            /// Create a buffer signature.
            /// \param[in] buffer Buffer whose signature to create.
            /// \param[in] bufferLength Buffer length.
            /// \return Buffer signature.
            util::Buffer SignBuffer (
                const void *buffer,
                std::size_t bufferLength);
            /// \brief
            /// Verify a buffer signature.
            /// \param[in] buffer Buffer whose signature to verify.
            /// \param[in] bufferLength Buffer length.
            /// \param[in] signature Signature to verify.
            /// \param[in] signatureLength Signature length.
            /// \return true == valid, false == invalid.
            bool VerifyBufferSignature (
                const void *buffer,
                std::size_t bufferLength,
                const void *signature,
                std::size_t signatureLength);

            /// \brief
            /// Create a file signature.
            /// \param[in] path File whose signature to create.
            /// \return File signature.
            util::Buffer SignFile (const std::string &path);
            /// \brief
            /// Verify a file signature.
            /// \param[in] path File whose signature to verify.
            /// \param[in] signature Signature to verify.
            /// \param[in] signatureLength Signature length.
            /// \return true == valid, false == invalid.
            bool VerifyFileSignature (
                const std::string &path,
                const void *signature,
                std::size_t signatureLength);

            /// \brief
            /// Authenticator is neither copy constructable, nor assignable.
            THEKOGANS_CRYPTO_DISALLOW_COPY_AND_ASSIGN (Authenticator)
        };

    } // namespace crypto
} // namespace thekogans

#endif // !defined (__thekogans_crypto_Authenticator_h)
