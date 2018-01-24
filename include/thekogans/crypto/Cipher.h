// Copyright 2016 Boris Kogan (boris@thekogans.net)
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

#if !defined (__thekogans_crypto_Cipher_h)
#define __thekogans_crypto_Cipher_h

#include <openssl/evp.h>
#include "thekogans/util/Types.h"
#include "thekogans/util/Flags.h"
#include "thekogans/util/Heap.h"
#include "thekogans/util/RefCounted.h"
#include "thekogans/util/Buffer.h"
#include "thekogans/util/FixedBuffer.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/crypto/Config.h"
#include "thekogans/crypto/SymmetricKey.h"
#include "thekogans/crypto/MAC.h"
#include "thekogans/crypto/FrameHeader.h"
#include "thekogans/crypto/CiphertextHeader.h"
#include "thekogans/crypto/OpenSSLUtils.h"

namespace thekogans {
    namespace crypto {

        /// \struct Cipher Cipher.h thekogans/crypto/Cipher.h
        ///
        /// \brief
        /// Cipher implements symmetric encryption/decryption using AES (CBC or GCM mode)
        /// Every encryption operation uses a random iv to thwart BEAST. MACs (CBC mode)
        /// are calculated over ciphertext to avoid the cryptographic doom principle:
        /// https://moxie.org/blog/the-cryptographic-doom-principle/. See the description
        /// of Cipher::Encrypt for more information.

        struct _LIB_THEKOGANS_CRYPTO_DECL Cipher : public util::ThreadSafeRefCounted {
            /// \brief
            /// Convenient typedef for util::ThreadSafeRefCounted::Ptr<Cipher>.
            typedef util::ThreadSafeRefCounted::Ptr<Cipher> Ptr;

            /// \struct Cipher::Stats Cipher.h thekogans/crypto/Cipher.h
            ///
            /// \brief
            /// Keeps track of usage statistics for various key components.
            struct _LIB_THEKOGANS_CRYPTO_DECL Stats {
                /// \brief
                /// Number of times this component was used.
                std::size_t useCount;
                /// \brief
                /// The shortest buffer this component saw.
                std::size_t minByteCount;
                /// \brief
                /// The longest buffer this component saw.
                std::size_t maxByteCount;
                /// \brief
                /// Total bytes processed by this component.
                std::size_t totalByteCount;

                /// \brief
                /// ctor.
                Stats () :
                    useCount (0),
                    minByteCount (0),
                    maxByteCount (0),
                    totalByteCount (0) {}

                /// \brief
                /// Update the usage statistics.
                /// \param[in] byteCount Current buffer length.
                void Update (std::size_t byteCount);

            #if defined (THEKOGANS_CRYPTO_TESTING)
                /// \brief
                /// "UseCount"
                static const char * const ATTR_USE_COUNT;
                /// \brief
                /// "MinByteCount"
                static const char * const ATTR_MIN_BYTE_COUNT;
                /// \brief
                /// "MaxByteCount"
                static const char * const ATTR_MAX_BYTE_COUNT;
                /// \brief
                /// "TotalByteCount"
                static const char * const ATTR_TOTAL_BYTE_COUNT;

                /// \brief
                /// Return the XML representation of stats.
                /// \param[in] indentationLevel How far to indent the leading tag.
                /// \param[in] tagName The name of the leading tag.
                /// \return XML representation of stats.
                std::string ToString (
                    util::ui32 indentationLevel,
                    const char *tagName) const;
            #endif // defined (THEKOGANS_CRYPTO_TESTING)
            };

        private:
            /// \brief
            /// \see{SymmetricKey} used to encrypt/decrypt.
            SymmetricKey::Ptr key;
            /// \brief
            /// OpenSSL cipher object.
            const EVP_CIPHER *cipher;
            /// \brief
            /// OpenSSL message digest object.
            const EVP_MD *md;
            /// \struct Cipher::Encryptor Cipher.h thekogans/crypto/Cipher.h
            ///
            /// \brief
            /// Encapsulates the encryption operation.
            struct Encryptor {
                /// \brief
                /// Cipher context used during encryption.
                CipherContext context;
                /// \brief
                /// Encryptor stats.
                Stats stats;

                /// \brief
                /// ctor.
                /// \param[in] key SymmetricKey used for encryption.
                /// \param[in] cipher Cipher used for encryption.
                Encryptor (
                    const SymmetricKey &key,
                    const EVP_CIPHER *cipher);

                /// \brief
                /// Return the length of the initialization vector (IV) associated with the cipher.
                /// \return The length of the initialization vector (IV) associated with the cipher.
                inline std::size_t GetIVLength () const {
                    return EVP_CIPHER_CTX_iv_length (&context);
                }

                /// \brief
                /// Generate a random iv.
                /// \param[out] iv Where to place the generated iv.
                /// \return Number of bytes written to iv.
                std::size_t GetIV (util::ui8 *iv) const;

                /// \brief
                /// Encrypt the plaintext.
                /// \param[in] plaintext Plaintext to encrypt.
                /// \param[in] plaintextLength Length of plaintext.
                /// \param[in] associatedData Optional associated data (GCM mode only).
                /// \param[in] associatedDataLength Length of optional associated data.
                /// \param[out] ivAndCiphertext Where to put the encrypted plaintext.
                /// \return Number of bytes written to ciphertext.
                std::size_t Encrypt (
                    const void *plaintext,
                    std::size_t plaintextLength,
                    const void *associatedData,
                    std::size_t associatedDataLength,
                    util::ui8 *ivAndCiphertext);

                /// \brief
                /// In GCM mode the cipher creates the mac for us. After
                /// calling Encrypt, call this method to get the mac (tag
                /// in GCM parlance).
                /// \param[out] tag Where to write the tag.
                /// \return Size of tag (in bytes).
                std::size_t GetTag (void *tag);

                /// \brief
                /// Encryptor is neither copy constructable, nor assignable.
                THEKOGANS_CRYPTO_DISALLOW_COPY_AND_ASSIGN (Encryptor)
            } encryptor;
            /// \struct Cipher::Decryptor Cipher.h thekogans/crypto/Cipher.h
            ///
            /// \brief
            /// Encapsulates the decryption operation.
            struct Decryptor {
                /// \brief
                /// Cipher context used during decryption.
                CipherContext context;
                /// \brief
                /// Decryptor stats.
                Stats stats;

                /// \brief
                /// ctor.
                /// \param[in] key SymmetricKey used for decryption.
                /// \param[in] cipher Cipher used for decryption.
                Decryptor (
                    const SymmetricKey &key,
                    const EVP_CIPHER *cipher);

                /// \brief
                /// Return the length of the initialization vector (IV) associated with the cipher.
                /// \return The length of the initialization vector (IV) associated with the cipher.
                inline std::size_t GetIVLength () const {
                    return EVP_CIPHER_CTX_iv_length (&context);
                }

                /// \brief
                /// In GCM mode the cipher needs the tag (produced during Encryptor::Encrypt).
                /// \param[in] tag Buffer containing the tag.
                /// \param[in] tagLength Length of buffer containing the tag.
                /// \return true.
                bool SetTag (
                    const void *tag,
                    std::size_t tagLength);

                /// \brief
                /// Decrypt the ciphertext.
                /// \param[in] ciphertext Ciphertext to decrypt.
                /// \param[in] ciphertextLength Length of ciphertext.
                /// \param[in] associatedData Optional associated data (GCM mode only).
                /// \param[in] associatedDataLength Length of optional associated data.
                /// \param[out] plaintext Where to put the decrypted ciphertext.
                /// \return Number of bytes written to plaintext.
                std::size_t Decrypt (
                    const void *ivAndCiphertext,
                    std::size_t ivAndCiphertextLength,
                    const void *associatedData,
                    std::size_t associatedDataLength,
                    util::ui8 *plaintext);

                /// \brief
                /// Decryptor is neither copy constructable, nor assignable.
                THEKOGANS_CRYPTO_DISALLOW_COPY_AND_ASSIGN (Decryptor)
            } decryptor;
            /// \brief
            /// \see{MAC} used to sign ciphertext in CBC mode.
            MAC::UniquePtr mac;

        public:
            /// \brief
            /// ctor.
            /// \param[in] key_ \see{SymmetricKey} used to encrypt/decrypt.
            /// \param[in] cipher OpenSSL EVP_CIPHER.
            /// \param[in] md OpenSSL EVP_MD (CBC mode only, ignored in GCM mode).
            Cipher (
                SymmetricKey::Ptr key_,
                const EVP_CIPHER *cipher_ = THEKOGANS_CRYPTO_DEFAULT_CIPHER,
                const EVP_MD *md_ = THEKOGANS_CRYPTO_DEFAULT_MD);

            enum {
                /// \brief
                /// Maximum plaintext length.
                MAX_PLAINTEXT_LENGTH =
                    util::UI32_MAX -
                    FrameHeader::SIZE -
                    CiphertextHeader::SIZE -
                    EVP_MAX_IV_LENGTH - // iv
                    EVP_MAX_BLOCK_LENGTH - // padding
                    EVP_MAX_MD_SIZE // mac
            };

            /// \brief
            /// Return the key length for a given cipher.
            /// \return Key length for a given cipher.
            static std::size_t GetKeyLength (
                const EVP_CIPHER *cipher = THEKOGANS_CRYPTO_DEFAULT_CIPHER);

            /// \brief
            /// Return the mode (EVP_CIPH_CBC_MODE or EVP_CIPH_GCM_MODE) for a given cipher.
            /// \return EVP_CIPH_CBC_MODE or EVP_CIPH_GCM_MODE.
            static util::i32 GetMode (
                const EVP_CIPHER *cipher = THEKOGANS_CRYPTO_DEFAULT_CIPHER);

            /// \brief
            /// Return true if the given cipher supports Authenticated Encryption with
            /// Associated Data (AEAD).
            /// \return true = AEAD cipher.
            static bool IsAEAD (
                const EVP_CIPHER *cipher = THEKOGANS_CRYPTO_DEFAULT_CIPHER);

            /// \brief
            /// Return max buffer length needed to encrypt the given amount of plaintext.
            /// \param[in] plaintextLength Amount of plaintext to encrypt.
            /// \return Max buffer length needed to encrypt the given amount of plaintext.
            static std::size_t GetMaxBufferLength (std::size_t plaintextLength);

            /// \brief
            /// Encrypt and mac plaintext. This is the workhorse encryption function
            /// used by others below. It writes the following structure in to ciphertext:
            ///
            /// |------------- CiphertextHeader -------------|--------- ciphertext ---------|
            /// +-----------+-------------------+------------+------+---------------+-------+
            /// | iv length | ciphertext length | mac length |  iv  |  ciphertext   |  mac  |
            /// +-----------+-------------------+------------+------+---------------+-------+
            /// |     2     |         4         |      2     | iv + ciphertext + mac length |
            ///
            /// \param[in] plaintext Plaintext to encrypt.
            /// \param[in] plaintextLength Plaintext length.
            /// \param[in] associatedData Optional associated data (GCM mode only).
            /// \param[in] associatedDataLength Length of optional associated data.
            /// \param[out] ciphertext Where to write encrypted ciphertext.
            /// \return Number of bytes written to ciphertext.
            std::size_t Encrypt (
                const void *plaintext,
                std::size_t plaintextLength,
                const void *associatedData,
                std::size_t associatedDataLength,
                util::ui8 *ciphertext);
            /// \beief
            /// Encrypt and mac plaintext. This function is a wrapper which allocates a buffer
            /// and calls Encrypt above.
            /// \param[in] plaintext Plaintext to encrypt.
            /// \param[in] plaintextLength Plaintext length.
            /// \param[in] associatedData Optional associated data (GCM mode only).
            /// \param[in] associatedDataLength Length of optional associated data.
            /// \return An encrypted and mac'ed buffer.
            util::Buffer::UniquePtr Encrypt (
                const void *plaintext,
                std::size_t plaintextLength,
                const void *associatedData = 0,
                std::size_t associatedDataLength = 0);

            /// \brief
            /// Encrypt, mac, and frame plaintext. It writes the following structure in to ciphertext:
            ///
            /// |-------- FrameHeader -------|------------- CiphertextHeader -------------|--------- ciphertext ---------|
            /// +--------+-------------------+-----------+-------------------+------------+------+---------------+-------+
            /// | key id | ciphertext length | iv length | ciphertext length | mac length |  iv  |  ciphertext   |  mac  |
            /// +--------+-------------------+-----------+-------------------+------------+------+---------------+-------+
            /// |   32   |         4         |     2     |         4         |      2     | iv + ciphertext + mac length |
            ///
            /// \param[in] plaintext Plaintext to encrypt.
            /// \param[in] plaintextLength Plaintext length.
            /// \param[in] associatedData Optional associated data (GCM mode only).
            /// \param[in] associatedDataLength Length of optional associated data.
            /// \param[out] ciphertext Where to write encrypted ciphertext.
            /// \return Number of bytes written to ciphertext.
            std::size_t EncryptAndFrame (
                const void *plaintext,
                std::size_t plaintextLength,
                const void *associatedData,
                std::size_t associatedDataLength,
                util::ui8 *ciphertext);
            /// \brief
            /// Encrypt, mac and frame plaintext. Similar to Encrypt above but allocates a
            /// buffer large enough to hold a \see{FrameHeader} containing the key id and
            /// ciphertext length and calls EncryptAndFrame above.
            /// \param[in] plaintext Plaintext to encrypt.
            /// \param[in] plaintextLength Plaintext length.
            /// \param[in] associatedData Optional associated data (GCM mode only).
            /// \param[in] associatedDataLength Length of optional associated data.
            /// \return An encrypted, mac'ed and framed buffer.
            util::Buffer::UniquePtr EncryptAndFrame (
                const void *plaintext,
                std::size_t plaintextLength,
                const void *associatedData = 0,
                std::size_t associatedDataLength = 0);

            /// \brief
            /// Verify the ciphertext MAC and, if matches, decrypt it.
            /// \param[in] ciphertext IV, ciphertext and MAC returned by one of Encrypt above.
            /// \param[in] ciphertextLength Length of ciphertext.
            /// \param[in] associatedData Optional associated data (GCM mode only).
            /// \param[in] associatedDataLength Length of optional associated data.
            /// \param[out] plaintext Where to write the decrypted plain text.
            /// \return Number of bytes written to plaintext.
            std::size_t Decrypt (
                const void *ciphertext,
                std::size_t ciphertextLength,
                const void *associatedData,
                std::size_t associatedDataLength,
                util::ui8 *plaintext);
            /// \brief
            /// Verify the ciphertext MAC and, if matches, decrypt it.
            /// \param[in] ciphertext IV, ciphertext and MAC returned by one of Encrypt above.
            /// \param[in] ciphertextLength Length of ciphertext.
            /// \param[in] associatedData Optional associated data (GCM mode only).
            /// \param[in] associatedDataLength Length of optional associated data.
            /// \param[in] secure true = return util::SecureBuffer.
            /// \param[in] endianness Resulting plaintext buffer endianness.
            /// \return Plaintext.
            util::Buffer::UniquePtr Decrypt (
                const void *ciphertext,
                std::size_t ciphertextLength,
                const void *associatedData = 0,
                std::size_t associatedDataLength = 0,
                bool secure = false,
                util::Endianness endianness = util::NetworkEndian);

            /// \brief
            /// Cipher is neither copy constructable, nor assignable.
            THEKOGANS_CRYPTO_DISALLOW_COPY_AND_ASSIGN (Cipher)
        };

    } // namespace crypto
} // namespace thekogans

#endif // !defined (__thekogans_crypto_Cipher_h)
