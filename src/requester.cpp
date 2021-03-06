/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2017-2020, Regents of the University of California.
 *
 * This file is part of ndncert, a certificate management system based on NDN.
 *
 * ndncert is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * ndncert is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received copies of the GNU General Public License along with
 * ndncert, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 *
 * See AUTHORS.md for complete list of ndncert authors and contributors.
 */

#include "requester.hpp"
#include "identity-challenge/challenge-module.hpp"
#include "detail/crypto-helper.hpp"
#include "detail/challenge-encoder.hpp"
#include "detail/error-encoder.hpp"
#include "detail/info-encoder.hpp"
#include "detail/new-renew-revoke-encoder.hpp"
#include "detail/probe-encoder.hpp"
#include <ndn-cxx/security/signing-helpers.hpp>
#include <ndn-cxx/security/transform/base64-encode.hpp>
#include <ndn-cxx/security/transform/buffer-source.hpp>
#include <ndn-cxx/security/transform/stream-sink.hpp>
#include <ndn-cxx/security/verification-helpers.hpp>
#include <ndn-cxx/util/io.hpp>
#include <ndn-cxx/util/random.hpp>
#include <ndn-cxx/metadata-object.hpp>
#include <boost/lexical_cast.hpp>

namespace ndn {
namespace ndncert {

NDN_LOG_INIT(ndncert.client);

shared_ptr<Interest>
Requester::genCaProfileDiscoveryInterest(const Name& caName)
{
  Name contentName = caName;
  if (readString(caName.at(-1)) != "CA")
    contentName.append("CA");
  contentName.append("INFO");
  return std::make_shared<Interest>(MetadataObject::makeDiscoveryInterest(contentName));
}

shared_ptr<Interest>
Requester::genCaProfileInterestFromDiscoveryResponse(const Data& reply)
{
  auto metaData = MetadataObject(reply);
  auto interestName= metaData.getVersionedName();
  interestName.appendSegment(0);
  auto interest = std::make_shared<Interest>(interestName);
  interest->setCanBePrefix(false);
  return interest;
}

boost::optional<CaProfile>
Requester::onCaProfileResponse(const Data& reply)
{
  auto caItem = InfoEncoder::decodeDataContent(reply.getContent());
  if (!security::verifySignature(reply, *caItem.m_cert)) {
    NDN_LOG_ERROR("Cannot verify replied Data packet signature.");
    NDN_THROW(std::runtime_error("Cannot verify replied Data packet signature."));
  }
  return caItem;
}


boost::optional<CaProfile>
Requester::onCaProfileResponseAfterRedirection(const Data& reply, const Name& caCertFullName)
{
  auto caItem = InfoEncoder::decodeDataContent(reply.getContent());
  auto certBlock = caItem.m_cert->wireEncode();
  caItem.m_cert = std::make_shared<security::Certificate>(certBlock);
  if (caItem.m_cert->getFullName() != caCertFullName) {
    NDN_LOG_ERROR("Ca profile does not match the certificate information offered by the original CA.");
    NDN_THROW(std::runtime_error("Cannot verify replied Data packet signature."));
  }
  return onCaProfileResponse(reply);
}

shared_ptr<Interest>
Requester::genProbeInterest(const CaProfile& ca, std::vector<std::tuple<std::string, std::string>>&& probeInfo)
{
  Name interestName = ca.m_caPrefix;
  interestName.append("CA").append("PROBE");
  auto interest =std::make_shared<Interest>(interestName);
  interest->setMustBeFresh(true);
  interest->setCanBePrefix(false);
  interest->setApplicationParameters(ProbeEncoder::encodeApplicationParameters(std::move(probeInfo)));
  return interest;
}

void
Requester::onProbeResponse(const Data& reply, const CaProfile& ca,
                           std::vector<std::pair<Name, int>>& identityNames, std::vector<Name>& otherCas)
{
  if (!security::verifySignature(reply, *ca.m_cert)) {
    NDN_LOG_ERROR("Cannot verify replied Data packet signature.");
    NDN_THROW(std::runtime_error("Cannot verify replied Data packet signature."));
    return;
  }
  processIfError(reply);
  ProbeEncoder::decodeDataContent(reply.getContent(), identityNames, otherCas);
}

shared_ptr<Interest>
Requester::genNewInterest(RequesterState& state, const Name& identityName,
                          const time::system_clock::TimePoint& notBefore,
                          const time::system_clock::TimePoint& notAfter)
{
  if (!state.m_caItem.m_caPrefix.isPrefixOf(identityName)) {
    return nullptr;
  }
  if (identityName.empty()) {
    NDN_LOG_TRACE("Randomly create a new name because identityName is empty and the param is empty.");
    state.m_identityName = state.m_caItem.m_caPrefix;
    state.m_identityName.append(std::to_string(random::generateSecureWord64()));
  }
  else {
    state.m_identityName = identityName;
  }

  // generate a newly key pair or use an existing key
  const auto& pib = state.m_keyChain.getPib();
  security::pib::Identity identity;
  try {
    identity = pib.getIdentity(state.m_identityName);
  }
  catch (const security::Pib::Error& e) {
    identity = state.m_keyChain.createIdentity(state.m_identityName);
    state.m_isNewlyCreatedIdentity = true;
    state.m_isNewlyCreatedKey = true;
  }
  try {
    state.m_keyPair = identity.getDefaultKey();
  }
  catch (const security::Pib::Error& e) {
    state.m_keyPair = state.m_keyChain.createKey(identity);
    state.m_isNewlyCreatedKey = true;
  }
  auto& keyName = state.m_keyPair.getName();

  // generate certificate request
  security::Certificate certRequest;
  certRequest.setName(Name(keyName).append("cert-request").appendVersion());
  certRequest.setContentType(ndn::tlv::ContentType_Key);
  certRequest.setContent(state.m_keyPair.getPublicKey().data(), state.m_keyPair.getPublicKey().size());
  SignatureInfo signatureInfo;
  signatureInfo.setValidityPeriod(security::ValidityPeriod(notBefore, notAfter));
  state.m_keyChain.sign(certRequest, signingByKey(keyName).setSignatureInfo(signatureInfo));

  // generate Interest packet
  Name interestName = state.m_caItem.m_caPrefix;
  interestName.append("CA").append("NEW");
  auto interest =std::make_shared<Interest>(interestName);
  interest->setMustBeFresh(true);
  interest->setCanBePrefix(false);
  interest->setApplicationParameters(
      NewRenewRevokeEncoder::encodeApplicationParameters(RequestType::NEW, state.m_ecdh.getBase64PubKey(), certRequest));

  // sign the Interest packet
  state.m_keyChain.sign(*interest, signingByKey(keyName));
  return interest;
}

shared_ptr<Interest>
Requester::genRevokeInterest(RequesterState& state, const security::Certificate& certificate)
{
  if (!state.m_caItem.m_caPrefix.isPrefixOf(certificate.getName())) {
    return nullptr;
  }
  // generate Interest packet
  Name interestName = state.m_caItem.m_caPrefix;
  interestName.append("CA").append("REVOKE");
  auto interest =std::make_shared<Interest>(interestName);
  interest->setMustBeFresh(true);
  interest->setCanBePrefix(false);
  interest->setApplicationParameters(
      NewRenewRevokeEncoder::encodeApplicationParameters(RequestType::REVOKE, state.m_ecdh.getBase64PubKey(), certificate));
  return interest;
}

std::list<std::string>
Requester::onNewRenewRevokeResponse(RequesterState& state, const Data& reply)
{
  if (!security::verifySignature(reply, *state.m_caItem.m_cert)) {
    NDN_LOG_ERROR("Cannot verify replied Data packet signature.");
    NDN_THROW(std::runtime_error("Cannot verify replied Data packet signature."));
  }
  processIfError(reply);

  auto contentTLV = reply.getContent();
  const auto& content = NewRenewRevokeEncoder::decodeDataContent(contentTLV);

  // ECDH and HKDF
  state.m_ecdh.deriveSecret(content.ecdhKey);
  hkdf(state.m_ecdh.m_sharedSecret, state.m_ecdh.m_sharedSecretLen,
       (uint8_t*)&content.salt, sizeof(content.salt), state.m_aesKey, sizeof(state.m_aesKey));

  // update state
  state.m_status = content.requestStatus;
  state.m_requestId = content.requestId;
  return content.challenges;
}

std::vector<std::tuple<std::string, std::string>>
Requester::selectOrContinueChallenge(RequesterState& state, const std::string& challengeSelected)
{
  auto challenge = ChallengeModule::createChallengeModule(challengeSelected);
  if (challenge == nullptr) {
    NDN_THROW(std::runtime_error("The challenge selected is not supported by your current version of NDNCERT."));
  }
  state.m_challengeType = challengeSelected;
  return challenge->getRequestedParameterList(state.m_status, state.m_challengeStatus);
}

shared_ptr<Interest>
Requester::genChallengeInterest(RequesterState& state,
                                std::vector<std::tuple<std::string, std::string>>&& parameters)
{
  if (state.m_challengeType == "") {
    NDN_THROW(std::runtime_error("The challenge has not been selected."));
  }
  auto challenge = ChallengeModule::createChallengeModule(state.m_challengeType);
  if (challenge == nullptr) {
    NDN_THROW(std::runtime_error("The challenge selected is not supported by your current version of NDNCERT."));
  }
  auto challengeParams = challenge->genChallengeRequestTLV(state.m_status, state.m_challengeStatus, std::move(parameters));

  Name interestName = state.m_caItem.m_caPrefix;
  interestName.append("CA").append("CHALLENGE").append(state.m_requestId);
  auto interest =std::make_shared<Interest>(interestName);
  interest->setMustBeFresh(true);
  interest->setCanBePrefix(false);

  // encrypt the Interest parameters
  auto paramBlock = encodeBlockWithAesGcm128(ndn::tlv::ApplicationParameters, state.m_aesKey,
                                             challengeParams.value(), challengeParams.value_size(),
                                             (const uint8_t*)state.m_requestId.c_str(),
                                             state.m_requestId.size(),
                                             state.m_aesBlockCounter);
  interest->setApplicationParameters(paramBlock);
  state.m_keyChain.sign(*interest, signingByKey(state.m_keyPair.getName()));
  return interest;
}

void
Requester::onChallengeResponse(RequesterState& state, const Data& reply)
{
  if (!security::verifySignature(reply, *state.m_caItem.m_cert)) {
    NDN_LOG_ERROR("Cannot verify replied Data packet signature.");
    NDN_THROW(std::runtime_error("Cannot verify replied Data packet signature."));
  }
  processIfError(reply);
  auto result = decodeBlockWithAesGcm128(reply.getContent(), state.m_aesKey,
                                         (const uint8_t*)state.m_requestId.c_str(),
                                         state.m_requestId.size());
  Block contentTLV = makeBinaryBlock(tlv::EncryptedPayload, result.data(), result.size());
  ChallengeEncoder::decodeDataContent(contentTLV, state);
}

shared_ptr<Interest>
Requester::genCertFetchInterest(const RequesterState& state)
{
  Name interestName = state.m_issuedCertName;
  auto interest =std::make_shared<Interest>(interestName);
  interest->setMustBeFresh(false);
  interest->setCanBePrefix(false);
  return interest;
}

shared_ptr<security::Certificate>
Requester::onCertFetchResponse(const Data& reply)
{
  try {
    return std::make_shared<security::Certificate>(reply);
  }
  catch (const std::exception& e) {
    NDN_LOG_ERROR("Cannot parse replied certificate ");
    NDN_THROW(std::runtime_error("Cannot parse replied certificate "));
    return nullptr;
  }
}

void
Requester::endSession(RequesterState& state)
{
  if (state.m_status == Status::SUCCESS || state.m_status == Status::ENDED) {
    return;
  }
  if (state.m_isNewlyCreatedIdentity) {
    // put the identity into the if scope is because it may cause an error
    // outside since when endSession is called, identity may not have been created yet.
    auto identity = state.m_keyChain.getPib().getIdentity(state.m_identityName);
    state.m_keyChain.deleteIdentity(identity);
  }
  else if (state.m_isNewlyCreatedKey) {
    auto identity = state.m_keyChain.getPib().getIdentity(state.m_identityName);
    state.m_keyChain.deleteKey(identity, state.m_keyPair);
  }
  state.m_status = Status::ENDED;
}

void
Requester::processIfError(const Data& data)
{
  auto errorInfo = ErrorEncoder::decodefromDataContent(data.getContent());
  if (std::get<0>(errorInfo) == ErrorCode::NO_ERROR) {
    return;
  }
  NDN_LOG_ERROR("Error info replied from the CA with Error code: " << std::get<0>(errorInfo) <<
                " and Error Info: " << std::get<1>(errorInfo));
  NDN_THROW(std::runtime_error("Error info replied from the CA with Error code: " +
                               boost::lexical_cast<std::string>(std::get<0>(errorInfo)) +
                               " and Error Info: " + std::get<1>(errorInfo)));
}

} // namespace ndncert
} // namespace ndn
