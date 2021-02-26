# Standard Auth

## Standard sign in
```YAML
Request:
type standard_sign_in_request = {
  "version": "1.0",
  "method": "standard_sign_in",
  "id": 2,
  "params": {
    "document": "{\"id\":\"did:elastos:iZikNnJkF8DvR72pW9Kc9GeRLMthhzQK1m\",\"publicKey\":[{\"id\":\"#primary\",\"publicKeyBase58\":\"fxh4cNz8WTCUCEfwnrXHvegMxt2ceXcE2uw1eXpKLtt1\"}],\"authentication\":[\"#primary\"],\"expires\":\"2025-12-24T15:16:10Z\",\"proof\":{\"created\":\"2020-12-24T15:16:10Z\",\"signatureValue\":\"r4z30I_G3hQEKgITHr9wvd1IHiXcPPBp3pBtjQ2UD3vwfpQX7IYihlBu6AA18qyQLWidO-ouEehVuJ52X-6ybw\"}}"
  }
}
Response:
type standard_sign_in_response = {
  "version": "1.0",
  "id": 2,
  "result": {
    "jwt_challenge": "eyJhbGciOiAiRVMyNTYiLCAidHlwIjogIkpXVCIsICJ2ZXJzaW9uIjogIjEuMCIsICJraWQiOiAiZGlkOmVsYXN0b3M6aXFKd3NuUkJlNldSUUVhTVRKd1NhcU1idmI5NTJYOFZCUyNwcmltYXJ5In0.eyJpc3MiOiJkaWQ6ZWxhc3RvczppcUp3c25SQmU2V1JRRWFNVEp3U2FxTWJ2Yjk1Mlg4VkJTIiwiZXhwIjoxNjEwNzIwNjIwLCJhdWQiOiJkaWQ6ZWxhc3RvczppWmlrTm5Ka0Y4RHZSNzJwVzlLYzlHZVJMTXRoaHpRSzFtIiwic3ViIjoiRElEQXV0aENoYWxsZW5nZSIsIm5hbWUiOiJ0ZXN0djEuMy4wIiwibm9uY2UiOiJEdTQ5R0FIWVc0MkdoU3laeFVXalIxN2FaNFB1RHZGQnAiLCJkZXNjcmlwdGlvbiI6InYxLjMuMCIsImVsYUFkZHJlc3MiOiIifQ.qsjrj39CQRVmyHdueAJa6jWUvV_d-PP1mU0ZMzBymbwY_Ij7AE_kteeliRSwvP6RnqiML2mHZ6LAaTCmxxEHXA"
  }
}
ErrorCode:
	(InvalidArgument, "Invalid argument.")
	(AuthBadDidDoc, "Bad did document.")
	(AuthDidDocInvlid, "Did document is invalid.")
	(AuthBadDid, "Did is invalid.")
	(AuthBadDidString, "Failed to convert did to string.")
	(AuthSaveDocFailed, "Failed to save document to local.")
	(AuthBadJwtBuilder, "Failed to get jwt builder from service did")
	(AuthBadJwtHeader, "Failed to set jwt header.")
	(AuthBadJwtExpiration, "Failed to set jwt expiration.")
	(AuthBadJwtAudience, "Failed to set jwt audience.")
	(AuthBadJwtSubject, "Failed to set jwt subject.")
	(AuthBadJwtClaim, "Failed to set jwt claim.")
	(AuthJwtSignFailed, "Failed to sign jwt.")
	(AuthJwtCompactFailed, "Failed to compact jwt.")
```



## Standard did auth

```YAML
Request:
type standard_did_auth_request = {
  "version": "1.0",
  "method": "standard_did_auth",
  "id": 1,
  "params": {
    "user_name": "test",
    "jwt_vp": "eyJ0eXAiOiJKV1QiLCJjdHkiOiJqc29uIiwiYWxnIjoiRVMyNTYifQ.eyJpc3MiOiJkaWQ6ZWxhc3RvczppZTJHdjRoTjR4aTZNNmljelk1TjhYM2ZIdFJTbmd2TkpjIiwiaWF0IjoxNjA3MDcyMDEwLCJleHAiOjE2MDcyNDQ4MTAsInByZXNlbnRhdGlvbiI6eyJ0eXBlIjoiVmVyaWZpYWJsZVByZXNlbnRhdGlvbiIsImNyZWF0ZWQiOiIyMDIwLTEyLTA0VDA4OjUzOjMwWiIsInZlcmlmaWFibGVDcmVkZW50aWFsIjpbeyJpZCI6ImRpZDplbGFzdG9zOmllMkd2NGhONHhpNk02aWN6WTVOOFgzZkh0UlNuZ3ZOSmMjYXBwLWlkLWNyZWRlbnRpYWwiLCJ0eXBlIjpbIkFwcElkQ3JlZGVudGlhbCJdLCJpc3N1ZXIiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwiaXNzdWFuY2VEYXRlIjoiMjAyMC0xMi0wNFQwODo1MzoyOFoiLCJleHBpcmF0aW9uRGF0ZSI6IjIwMjEtMDEtMDNUMDg6NTM6MjhaIiwiY3JlZGVudGlhbFN1YmplY3QiOnsiaWQiOiJkaWQ6ZWxhc3RvczppZTJHdjRoTjR4aTZNNmljelk1TjhYM2ZIdFJTbmd2TkpjIiwiYXBwRGlkIjoiZGlkOmVsYXN0b3M6dG9kbyIsImFwcEluc3RhbmNlRGlkIjoiZGlkOmVsYXN0b3M6aWUyR3Y0aE40eGk2TTZpY3pZNU44WDNmSHRSU25ndk5KYyJ9LCJwcm9vZiI6eyJ0eXBlIjoiRUNEU0FzZWNwMjU2cjEiLCJ2ZXJpZmljYXRpb25NZXRob2QiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyI3ByaW1hcnkiLCJzaWduYXR1cmUiOiJiYTdER3BYZm9VRlg2NGdJMHEzem1kZ0d5ZERXeHRnWGlEUHJKTkFZU29VbVNod01xdTFoWk55SUJ6R0hKbnh6U0tYUWh3YkdVem9aZ1A1VnNmTmpUQSJ9fV0sInByb29mIjp7InR5cGUiOiJFQ0RTQXNlY3AyNTZyMSIsInZlcmlmaWNhdGlvbk1ldGhvZCI6ImRpZDplbGFzdG9zOmllMkd2NGhONHhpNk02aWN6WTVOOFgzZkh0UlNuZ3ZOSmMjcHJpbWFyeSIsInJlYWxtIjoiZGlkOmVsYXN0b3M6aXFKd3NuUkJlNldSUUVhTVRKd1NhcU1idmI5NTJYOFZCUyIsIm5vbmNlIjoiM3Z5ZnVLOFpHSm45YWJ3c2pxZ0JYc1VhUDVYMnRDSGFTIiwic2lnbmF0dXJlIjoiUjNqcHVIaWg0QUZJQjhfMVVKQm1qQi1Kd2NzbmpvSHBMZFJ4ZnZQY0g3YXZJZjNoLXllZ0lTU2RESzdLdFlXZWJOWnJVTW5uOEFTYUg5V0IzcFRzUEEifX19.5E0jF2kxlNqhDhd8R9oBPNbAFhFFnzQzKEedQz4hyCPtfgwio7zTXgezwSTh7wFquxdapog-KtW3PjG4t4RKdA"
  }
}
Response:
type standard_did_auth_response = {
  "version": "1.0",
  "id": 3,
  "result": {
    "access_token": "eyJhbGciOiAiRVMyNTYiLCAidHlwIjogIkpXVCIsICJ2ZXJzaW9uIjogIjEuMCIsICJraWQiOiAiZGlkOmVsYXN0b3M6aXFKd3NuUkJlNldSUUVhTVRKd1NhcU1idmI5NTJYOFZCUyNwcmltYXJ5In0.eyJpc3MiOiJkaWQ6ZWxhc3RvczppcUp3c25SQmU2V1JRRWFNVEp3U2FxTWJ2Yjk1Mlg4VkJTIiwiZXhwIjoxNjExNDE1MDg5LCJhdWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwic3ViIjoiQWNjZXNzVG9rZW4iLCJuYW1lIjoidGVzdCIsInVzZXJEaWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwiZW1haWwiOiJOQSIsImFwcElkIjoiZGlkOmVsYXN0b3M6aXF0V1JWano3Z3NZaHl1UUViMWhZTk5tV1F0MVo5Z2VYZyIsImFwcEluc3RhbmNlRGlkIjoiZGlkOmVsYXN0b3M6aW1aZ0FvOVczOFZ6bzFwSlFmSHA2TkpwOUxac3JuUlBSciIsInVpZCI6MX0._WU28rPrb0ixLBlk0j5_OLJTqh4Lp2e9pbM4yorY8nIHXHjfZMw9jT2oc9pOVt0AJ3CiNRMd7enrWJ2XuBP5LA"
  }
}
ErrorCode:
	(InvalidArgument, "Invalid argument")
	(AuthBadJwtChallenge, "Failed to parse jws from jwt.")
	(AuthGetJwsClaimFailed, "Failed to get claim from jws.")
	(AuthGetPresentationFailed, "Failed to get presentation from json.")
	(AuthInvalidPresentation, "Failed to check presentation.")
	(AuthPresentationEmptyNonce, "Failed to get presentation nonce, return null.")
	(AuthPresentationBadNonce, "Bad presentation nonce.")
	(AuthPresentationEmptyRealm, "Failed to get presentation realm, return null.")
	(AuthPresentationBadRealm, "Bad presentation realm.")
	(AuthVerifiableCredentialBadCount, "The credential count is error.")
	(AuthCredentialNotExists, "The credential isn't exist.")
	(AuthCredentialParseFailed, "The credential string is error, unable to rebuild to a credential object.")
	(AuthCredentialInvalid, "The credential isn't valid.")
	(AuthCredentialIdNotExists, "The credential id isn't exist.")
	(AuthCredentialBadInstanceId, "Bad instance id.")
	(AuthCredentialPropertyNotExists, "The credential property isn't exist.")
	(AuthCredentialPropertyAppIdNotExists, "The credential subject's id isn't exist.")
	(AuthNonceExpiredError, "Nonce is expired.")
	(AuthCredentialExpirationError, "Faile to get credential expiration date.")
	(AuthUpdateOwnerError, "Failed to update owner.")
	(AuthUpdateUserError, "Failed to update follower.")
	(AuthBadJwtBuilder, "Failed to get jwt builder from service did")
	(AuthBadJwtHeader, "Failed to set jwt header.")
	(AuthBadJwtExpiration, "Failed to set jwt expiration.")
	(AuthBadJwtAudience, "Failed to set jwt audience.")
	(AuthBadJwtSubject, "Failed to set jwt subject.")
	(AuthBadJwtClaim, "Failed to set jwt claim.")
	(AuthJwtSignFailed, "Failed to sign jwt.")
	(AuthJwtCompactFailed, "Failed to compact jwt.")
```