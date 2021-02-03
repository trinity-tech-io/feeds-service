# Legacy RPC

## Declare owner
```YAML
Request:
type declare_owner_request = {
  "version": "1.0"
  "method": "declare_owner"
  "id": 1
  "params": {
    "nonce": "HDsfGnG8EyMV6arVYgCvvcDii4ZxAEjU1"
    "owner_did": "did:elastos:imZgAo9W38Vzo1pJQfHp6NJp9LZsrnRPRr"
  }
}
Response:
type declare_owner_response = {
  "version": "1.0",
  "id": 1,
  "result": {
    "phase": "owner_declared" 
  }
}
OR
type declare_owner_response = {
  "version": "1.0",
  "id": 1,
  "result": {
    "phase": "did_imported",
    "did": "did:elastos:iqJwsnRBe6WRQEaMTJwSaqMbvb952X8VBS",
    "transaction_payload": "{\"header\":{\"specification\":\"elastos/did/1.0\",\"operation\":\"create\"},\"payload\":\"eyJpZCI6ImRpZDplbGFzdG9zOmlxSndzblJCZTZXUlFFYU1USndTYXFNYnZiOTUyWDhWQlMiLCJwdWJsaWNLZXkiOlt7ImlkIjoiI3ByaW1hcnkiLCJwdWJsaWNLZXlCYXNlNTgiOiJkU1hBVFQ3R0hEdW15WTZoNDM5NEJyUUIyS2NHWVJOelFBak5UWmpYU2F4MSJ9XSwiYXV0aGVudGljYXRpb24iOlsiI3ByaW1hcnkiXSwiZXhwaXJlcyI6IjIwMjUtMTEtMTVUMTg6NTA6MjlaIiwicHJvb2YiOnsiY3JlYXRlZCI6IjIwMjAtMTEtMTZUMDI6NTA6MjlaIiwic2lnbmF0dXJlVmFsdWUiOiJteDljLXFLNk0zaEhWVVhnM0tZVTVaYXJiN2hpdkxTM0dtVHF4a1ktN3BSWFc4bnJQME9oR0FSU0U4Y29TOUR0RjdMbktlQVl4c1FqVV9aRWN0TUN4USJ9fQ\",\"proof\":{\"verificationMethod\":\"#primary\",\"signature\":\"7RqcgArL3qLAFUuAzgSUk0foV8QWlr3jobLaJDYGaU7KX4aTlW0-4uPxEhPnTqQzdlYhVL9XVPmMLcozqbGWZA\"}}"
  }
}
OR
type declare_owner_response = {
  "version": "1.0",
  "id": 1,
  "result": {
    "phase": "credential_issued" 
  }
}
ErrorCode:
	(ERR_INTERNAL_ERROR, "Feeds service internal error.")
	(ERR_NOT_AUTHORIZED, "Feeds service is not authorized.")
```



## Import did

```YAML
Request:
type import_did_request = {
  "version": "1.0"
  "method": "import_did"
  "id": 1
  "params": {
    "mnemonic": "",
    "passphrase": "",
    "index": 0
  }
}
Response:
type import_did_response = {
  "version": "1.0",
  "id": 1,
  "result": {
    "phase": "did_imported",
    "did": "did:elastos:iqJwsnRBe6WRQEaMTJwSaqMbvb952X8VBS",
    "transaction_payload": "{\"header\":{\"specification\":\"elastos/did/1.0\",\"operation\":\"create\"},\"payload\":\"eyJpZCI6ImRpZDplbGFzdG9zOmlxSndzblJCZTZXUlFFYU1USndTYXFNYnZiOTUyWDhWQlMiLCJwdWJsaWNLZXkiOlt7ImlkIjoiI3ByaW1hcnkiLCJwdWJsaWNLZXlCYXNlNTgiOiJkU1hBVFQ3R0hEdW15WTZoNDM5NEJyUUIyS2NHWVJOelFBak5UWmpYU2F4MSJ9XSwiYXV0aGVudGljYXRpb24iOlsiI3ByaW1hcnkiXSwiZXhwaXJlcyI6IjIwMjUtMTEtMTVUMTg6NTA6MjlaIiwicHJvb2YiOnsiY3JlYXRlZCI6IjIwMjAtMTEtMTZUMDI6NTA6MjlaIiwic2lnbmF0dXJlVmFsdWUiOiJteDljLXFLNk0zaEhWVVhnM0tZVTVaYXJiN2hpdkxTM0dtVHF4a1ktN3BSWFc4bnJQME9oR0FSU0U4Y29TOUR0RjdMbktlQVl4c1FqVV9aRWN0TUN4USJ9fQ\",\"proof\":{\"verificationMethod\":\"#primary\",\"signature\":\"7RqcgArL3qLAFUuAzgSUk0foV8QWlr3jobLaJDYGaU7KX4aTlW0-4uPxEhPnTqQzdlYhVL9XVPmMLcozqbGWZA\"}}"
  }
}
ErrorCode:
	(ERR_INTERNAL_ERROR, "Feeds service internal error.")
	(ERR_NOT_AUTHORIZED, "Feeds service is not authorized.")
```



## Issue credential

```YAML
Request:
type issue_credential_request = {
  "version": "1.0"
  "method": "issue_credential"
  "id": 1
  "params": {
    "credential": "{\"id\":\"did:elastos:iqJwsnRBe6WRQEaMTJwSaqMbvb952X8VBS#credential\",\"type\":[\"BasicProfileCredential\"],\"issuer\":\"did:elastos:imZgAo9W38Vzo1pJQfHp6NJp9LZsrnRPRr\",\"issuanceDate\":\"2020-11-16T02:51:16.000Z\",\"expirationDate\":\"2024-11-09T02:51:16.000Z\",\"credentialSubject\":{\"id\":\"did:elastos:iqJwsnRBe6WRQEaMTJwSaqMbvb952X8VBS\",\"description\":\"v1.3.0\",\"elaAddress\":\"\",\"name\":\"testv1.3.0\"},\"proof\":{\"type\":\"ECDSAsecp256r1\",\"verificationMethod\":\"did:elastos:imZgAo9W38Vzo1pJQfHp6NJp9LZsrnRPRr#primary\",\"signature\":\"55LTR3rnqQfwiMEo9Ls8v00HlpeYmqxsPGJomilFLPXxcs6cebzd6bxwSp78f5ydRVDzMOLia4jT16b2vsq2aQ\"}}"
  }
}
Response:
type issue_credential_response = {
  "version": "1.0",
  "id": 1,
}
ErrorCode:
	(ERR_INTERNAL_ERROR, "Feeds service internal error.")
	(ERR_WRONG_STATE, "Feeds service is wrong state to issue credential.")
	(ERR_INVALID_PARAMS, "Invalid params.")
```



## Update credential

```YAML
Request:
type update_credential_request = {
  "version": "1.0"
  "method": "update_credential"
  "id": 1
  "params": {
  	"access_token": "eyJhbGciOiAiRVMyNTYiLCAidHlwIjogIkpXVCIsICJ2ZXJzaW9uIjogIjEuMCIsICJraWQiOiAiZGlkOmVsYXN0b3M6aXFKd3NuUkJlNldSUUVhTVRKd1NhcU1idmI5NTJYOFZCUyNwcmltYXJ5In0.eyJpc3MiOiJkaWQ6ZWxhc3RvczppcUp3c25SQmU2V1JRRWFNVEp3U2FxTWJ2Yjk1Mlg4VkJTIiwiZXhwIjoxNjE0MTM0NjkwLCJhdWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwic3ViIjoiQWNjZXNzVG9rZW4iLCJuYW1lIjoidGVzdCIsInVzZXJEaWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwiZW1haWwiOiJOQSIsImFwcElkIjoiZGlkOmVsYXN0b3M6aXF0V1JWano3Z3NZaHl1UUViMWhZTk5tV1F0MVo5Z2VYZyIsImFwcEluc3RhbmNlRGlkIjoiZGlkOmVsYXN0b3M6aW1aZ0FvOVczOFZ6bzFwSlFmSHA2TkpwOUxac3JuUlBSciIsInVpZCI6MX0.vVk3WZ4jrBvSXssmCK6sVDJataJUMbpwg3jBPyJeRCJG4rnQUlcetXyh1EZwxdUzWAjsTA81-8eecp9CNgJbdQ",
    "credential": "{\"id\":\"did:elastos:iqJwsnRBe6WRQEaMTJwSaqMbvb952X8VBS#credential\",\"type\":[\"BasicProfileCredential\"],\"issuer\":\"did:elastos:imZgAo9W38Vzo1pJQfHp6NJp9LZsrnRPRr\",\"issuanceDate\":\"2020-11-16T02:51:16.000Z\",\"expirationDate\":\"2024-11-09T02:51:16.000Z\",\"credentialSubject\":{\"id\":\"did:elastos:iqJwsnRBe6WRQEaMTJwSaqMbvb952X8VBS\",\"description\":\"v1.3.0\",\"elaAddress\":\"\",\"name\":\"testv1.3.0\"},\"proof\":{\"type\":\"ECDSAsecp256r1\",\"verificationMethod\":\"did:elastos:imZgAo9W38Vzo1pJQfHp6NJp9LZsrnRPRr#primary\",\"signature\":\"55LTR3rnqQfwiMEo9Ls8v00HlpeYmqxsPGJomilFLPXxcs6cebzd6bxwSp78f5ydRVDzMOLia4jT16b2vsq2aQ\"}}"
  }
}
Response:
type update_credential_response = {
  "version": "1.0",
  "id": 1,
}
ErrorCode:
	(ERR_INTERNAL_ERROR, "Feeds service internal error.")
	(ERR_WRONG_STATE, "Feeds service is wrong state to issue credential.")
	(ERR_INVALID_PARAMS, "Invalid params.")
	(ERR_ACCESS_TOKEN_EXP, "Access token invalid.")
	(ERR_NOT_AUTHORIZED, Feeds service is not authorized.")
```



## Signin request challenge

```YAML
Request:
type signin_request_challenge_request = {
  "version": "1.0"
  "method": "signin_request_challenge"
  "id": 1
  "params": {
    "credential_required": true,
    "jws": "eyJhbGciOiAiRVMyNTYiLCAia2lkIjogImRpZDplbGFzdG9zOmlxSndzblJCZTZXUlFFYU1USndTYXFNYnZiOTUyWDhWQlMjcHJpbWFyeSJ9.eyJpc3MiOiAiZGlkOmVsYXN0b3M6aXFKd3NuUkJlNldSUUVhTVRKd1NhcU1idmI5NTJYOFZCUyIsICJzdWIiOiAiZGlkYXV0aCIsICJyZWFsbSI6ICJDcVlTRXRYVTIxS3NRUU14OUQ4eTNScG9lNjU1OU5FMzg0UWo2ajk1VjFwSiIsICJub25jZSI6ICJCUno0amJXQWRwUHJid3dxeU5Yc0hFUVZSRjlNcEhOS1gifQ.WvDRIw2krKBcHUQhnmJlj-fIXbWW8ggSy5m9ih5TzeAVXQjPIe5AMzgFl4YliHKOcfPtO7sh4UXTm4DEsEoB1A"
  }
}
Response:
type signin_request_challenge_response = {
  "version": "1.0",
  "id": 1,
  "result": {
    "credential_required": true,
    "jws": "eyJhbGciOiAiRVMyNTYiLCAia2lkIjogImRpZDplbGFzdG9zOmlxSndzblJCZTZXUlFFYU1USndTYXFNYnZiOTUyWDhWQlMjcHJpbWFyeSJ9.eyJpc3MiOiAiZGlkOmVsYXN0b3M6aXFKd3NuUkJlNldSUUVhTVRKd1NhcU1idmI5NTJYOFZCUyIsICJzdWIiOiAiZGlkYXV0aCIsICJyZWFsbSI6ICJDcVlTRXRYVTIxS3NRUU14OUQ4eTNScG9lNjU1OU5FMzg0UWo2ajk1VjFwSiIsICJub25jZSI6ICJCUno0amJXQWRwUHJid3dxeU5Yc0hFUVZSRjlNcEhOS1gifQ.WvDRIw2krKBcHUQhnmJlj-fIXbWW8ggSy5m9ih5TzeAVXQjPIe5AMzgFl4YliHKOcfPtO7sh4UXTm4DEsEoB1A"
  }
}
ErrorCode:
	(ERR_INTERNAL_ERROR, "Feeds service internal error.")
	(ERR_INVALID_PARAMS, "Invalid params.")
	
```



## Signin confirm challenge

```YAML
Request:
type signin_confirm_challenge_request = {
  "version": "1.0"
  "method": "signin_confirm_challenge"
  "id": 1
  "params": {
    "jws": "eyJ0eXAiOiJKV1QiLCJjdHkiOiJqc29uIiwiYWxnIjoiRVMyNTYifQ.eyJpc3MiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwiaWF0IjoxNjA1NDk1MDgxLCJleHAiOjE2MDU0OTUzODEsInByZXNlbnRhdGlvbiI6eyJ0eXBlIjoiVmVyaWZpYWJsZVByZXNlbnRhdGlvbiIsImNyZWF0ZWQiOiIyMDIwLTExLTE2VDAyOjUxOjIxWiIsInZlcmlmaWFibGVDcmVkZW50aWFsIjpbXSwicHJvb2YiOnsidHlwZSI6IkVDRFNBc2VjcDI1NnIxIiwidmVyaWZpY2F0aW9uTWV0aG9kIjoiZGlkOmVsYXN0b3M6aW1aZ0FvOVczOFZ6bzFwSlFmSHA2TkpwOUxac3JuUlBSciNwcmltYXJ5IiwicmVhbG0iOiJDcVlTRXRYVTIxS3NRUU14OUQ4eTNScG9lNjU1OU5FMzg0UWo2ajk1VjFwSiIsIm5vbmNlIjoiQlJ6NGpiV0FkcFByYnd3cXlOWHNIRVFWUkY5TXBITktYIiwic2lnbmF0dXJlIjoiSlZXemdJc004angxaHVHbzdhaTZFM0JSYnNWLU96ZldGLTNhcGRvck1zMmNFV2ExQ2NHX05haTN6TzlGOXFUcDk5OGdFRW1NM3MxY3hQcVpLWTM0dFEifX19.JGcuN29tyEqlq6j-vA1bDetZto7aj-J7K49gswrZHnD31YieiTw0NV5gxbEdyblzbvbtkBuQq0pV2ExIkAfYTQ"
    }
}
Response:
type signin_confirm_challenge_response = {
  "version": "1.0",
  "id": 1,
  "result": {
    "access_token": "eyJhbGciOiAiRVMyNTYiLCAia2lkIjogImRpZDplbGFzdG9zOmlxSndzblJCZTZXUlFFYU1USndTYXFNYnZiOTUyWDhWQlMjcHJpbWFyeSJ9.eyJpc3MiOiAiZGlkOmVsYXN0b3M6aXFKd3NuUkJlNldSUUVhTVRKd1NhcU1idmI5NTJYOFZCUyIsICJzdWIiOiAiZGlkOmVsYXN0b3M6aW1aZ0FvOVczOFZ6bzFwSlFmSHA2TkpwOUxac3JuUlBSciIsICJleHAiOiAxNjEwNjc5MDgyLCAidWlkIjogMSwgIm5hbWUiOiAidGVzdCIsICJlbWFpbCI6ICJOQSJ9.dY1N2415mP4A8jTrFaeEGp8dzZHQ4D3FUNWr3Wm75b3x0VxI-TDZGKT72v84VyAGKFJZVYb4CtKbl86DWGMRZw",
    "exp": 1610679082
  }
}
ErrorCode:
	(ERR_INTERNAL_ERROR, "Feeds service internal error.")
	(ERR_INVALID_PARAMS, "Invalid params.")
	(ERR_INVALID_CHAL_RESP, "Invalid challenge."
	(ERR_INVALID_VC, "Invalid vc."
```



## Create channel

```YAML
Request:
type create_channel_request = {
  "version": "1.0"
  "method": "create_channel"
  "id": 1
  "params": {
    "access_token": "eyJhbGciOiAiRVMyNTYiLCAia2lkIjogImRpZDplbGFzdG9zOmlxSndzblJCZTZXUlFFYU1USndTYXFNYnZiOTUyWDhWQlMjcHJpbWFyeSJ9.eyJpc3MiOiAiZGlkOmVsYXN0b3M6aXFKd3NuUkJlNldSUUVhTVRKd1NhcU1idmI5NTJYOFZCUyIsICJzdWIiOiAiZGlkOmVsYXN0b3M6aW1aZ0FvOVczOFZ6bzFwSlFmSHA2TkpwOUxac3JuUlBSciIsICJleHAiOiAxNjEwNjc5MDgyLCAidWlkIjogMSwgIm5hbWUiOiAidGVzdCIsICJlbWFpbCI6ICJOQSJ9.dY1N2415mP4A8jTrFaeEGp8dzZHQ4D3FUNWr3Wm75b3x0VxI-TDZGKT72v84VyAGKFJZVYb4CtKbl86DWGMRZw",
    "name": "test12",
    "introduction": "test12345678",
    "avatar": Any
  }
}
Response:
type create_channel_response = {
  "version": "1.0",
  "id": 1,
  "result": {
    "id": 1
  }
}
ErrorCode:
	(ERR_ACCESS_TOKEN_EXP, "Access token invalid.")
	(ERR_NOT_AUTHORIZED, "Feeds service is not authorized.")
	(ERR_ALREADY_EXISTS, "Channel has been exists."
	(ERR_INTERNAL_ERROR, "Feeds service internal error.")
	
```



## Update feedinfo

```YAML
Request:
type update_feedinfo_request = {
  "version": "1.0"
  "method": "update_feedinfo"
  "id": 1
  "params": {
    "access_token": "eyJhbGciOiAiRVMyNTYiLCAidHlwIjogIkpXVCIsICJ2ZXJzaW9uIjogIjEuMCIsICJraWQiOiAiZGlkOmVsYXN0b3M6aXFKd3NuUkJlNldSUUVhTVRKd1NhcU1idmI5NTJYOFZCUyNwcmltYXJ5In0.eyJpc3MiOiJkaWQ6ZWxhc3RvczppcUp3c25SQmU2V1JRRWFNVEp3U2FxTWJ2Yjk1Mlg4VkJTIiwiZXhwIjoxNjExNDE1MDg5LCJhdWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwic3ViIjoiQWNjZXNzVG9rZW4iLCJuYW1lIjoidGVzdCIsInVzZXJEaWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwiZW1haWwiOiJOQSIsImFwcElkIjoiZGlkOmVsYXN0b3M6aXF0V1JWano3Z3NZaHl1UUViMWhZTk5tV1F0MVo5Z2VYZyIsImFwcEluc3RhbmNlRGlkIjoiZGlkOmVsYXN0b3M6aW1aZ0FvOVczOFZ6bzFwSlFmSHA2TkpwOUxac3JuUlBSciIsInVpZCI6MX0.sw4hUdL65CkLZp_V3xDjsp8U60sOsZHOqwqa-DnLEmhRUfSkC5JZ2_3uDl74KnZj-86yYDxkCsO9PLyqWrUT0A",
    "id": 6,
    "name": "test21",
    "introduction": "test2121",
    "avatar": Any
  }
}
Response:
type update_feedinfo_response = {
  "version": "1.0",
  "id": 1,
}
ErrorCode:
	(ERR_ACCESS_TOKEN_EXP, "Access token invalid.")
	(ERR_NOT_AUTHORIZED, "Feeds service is not authorized.")
	(ERR_NOT_EXISTS, "Channel has not been created."
	(ERR_INTERNAL_ERROR, "Feeds service internal error.")	
```



## Publish post

```YAML
Request:
type publish_post_request = {
  "version": "1.0"
  "method": "publish_post"
  "id": 1
  "params": {
    "access_token": "eyJhbGciOiAiRVMyNTYiLCAidHlwIjogIkpXVCIsICJ2ZXJzaW9uIjogIjEuMCIsICJraWQiOiAiZGlkOmVsYXN0b3M6aXFKd3NuUkJlNldSUUVhTVRKd1NhcU1idmI5NTJYOFZCUyNwcmltYXJ5In0.eyJpc3MiOiJkaWQ6ZWxhc3RvczppcUp3c25SQmU2V1JRRWFNVEp3U2FxTWJ2Yjk1Mlg4VkJTIiwiZXhwIjoxNjE0MTM0NjkwLCJhdWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwic3ViIjoiQWNjZXNzVG9rZW4iLCJuYW1lIjoidGVzdCIsInVzZXJEaWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwiZW1haWwiOiJOQSIsImFwcElkIjoiZGlkOmVsYXN0b3M6aXF0V1JWano3Z3NZaHl1UUViMWhZTk5tV1F0MVo5Z2VYZyIsImFwcEluc3RhbmNlRGlkIjoiZGlkOmVsYXN0b3M6aW1aZ0FvOVczOFZ6bzFwSlFmSHA2TkpwOUxac3JuUlBSciIsInVpZCI6MX0.vVk3WZ4jrBvSXssmCK6sVDJataJUMbpwg3jBPyJeRCJG4rnQUlcetXyh1EZwxdUzWAjsTA81-8eecp9CNgJbdQ",
    "channel_id": 1,
    "content": Any
  }
}
Response:
type publish_post_response = {
  "version": "1.0",
  "id": 1,
  "result": {
    "id": 95
  }
}
ErrorCode:
	(ERR_ACCESS_TOKEN_EXP, "Access token invalid.")
	(ERR_NOT_AUTHORIZED, "Feeds service is not authorized.")
	(ERR_NOT_EXISTS, "Channel is not exists."
	(ERR_INTERNAL_ERROR, "Feeds service internal error.")	
```



## Declare post

```YAML
Request:
type declare_post_request = {
  "version": "1.0"
  "method": "declare_post"
  "id": 1
  "params": {
    "access_token": "eyJhbGciOiAiRVMyNTYiLCAidHlwIjogIkpXVCIsICJ2ZXJzaW9uIjogIjEuMCIsICJraWQiOiAiZGlkOmVsYXN0b3M6aXFKd3NuUkJlNldSUUVhTVRKd1NhcU1idmI5NTJYOFZCUyNwcmltYXJ5In0.eyJpc3MiOiJkaWQ6ZWxhc3RvczppcUp3c25SQmU2V1JRRWFNVEp3U2FxTWJ2Yjk1Mlg4VkJTIiwiZXhwIjoxNjE0MTM0NjkwLCJhdWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwic3ViIjoiQWNjZXNzVG9rZW4iLCJuYW1lIjoidGVzdCIsInVzZXJEaWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwiZW1haWwiOiJOQSIsImFwcElkIjoiZGlkOmVsYXN0b3M6aXF0V1JWano3Z3NZaHl1UUViMWhZTk5tV1F0MVo5Z2VYZyIsImFwcEluc3RhbmNlRGlkIjoiZGlkOmVsYXN0b3M6aW1aZ0FvOVczOFZ6bzFwSlFmSHA2TkpwOUxac3JuUlBSciIsInVpZCI6MX0.vVk3WZ4jrBvSXssmCK6sVDJataJUMbpwg3jBPyJeRCJG4rnQUlcetXyh1EZwxdUzWAjsTA81-8eecp9CNgJbdQ",
    "channel_id": 1,
    "content": Any,
    "with_notify": false
  }
}
Response:
type declare_post_response = {
  "version": "1.0",
  "id": 1,
  "result": {
    "id": 95
  }
}
ErrorCode:
	(ERR_ACCESS_TOKEN_EXP, "Access token invalid.")
	(ERR_NOT_AUTHORIZED, "Feeds service is not authorized.")
	(ERR_NOT_EXISTS, "Channel is not exists."
	(ERR_INTERNAL_ERROR, "Feeds service internal error.")	
```



## Notify post

```YAML
Request:
type notify_post_request = {
  "version": "1.0"
  "method": "notify_post"
  "id": 1
  "params": {
    "access_token": "eyJhbGciOiAiRVMyNTYiLCAidHlwIjogIkpXVCIsICJ2ZXJzaW9uIjogIjEuMCIsICJraWQiOiAiZGlkOmVsYXN0b3M6aXFKd3NuUkJlNldSUUVhTVRKd1NhcU1idmI5NTJYOFZCUyNwcmltYXJ5In0.eyJpc3MiOiJkaWQ6ZWxhc3RvczppcUp3c25SQmU2V1JRRWFNVEp3U2FxTWJ2Yjk1Mlg4VkJTIiwiZXhwIjoxNjE0MTM0NjkwLCJhdWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwic3ViIjoiQWNjZXNzVG9rZW4iLCJuYW1lIjoidGVzdCIsInVzZXJEaWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwiZW1haWwiOiJOQSIsImFwcElkIjoiZGlkOmVsYXN0b3M6aXF0V1JWano3Z3NZaHl1UUViMWhZTk5tV1F0MVo5Z2VYZyIsImFwcEluc3RhbmNlRGlkIjoiZGlkOmVsYXN0b3M6aW1aZ0FvOVczOFZ6bzFwSlFmSHA2TkpwOUxac3JuUlBSciIsInVpZCI6MX0.vVk3WZ4jrBvSXssmCK6sVDJataJUMbpwg3jBPyJeRCJG4rnQUlcetXyh1EZwxdUzWAjsTA81-8eecp9CNgJbdQ",
    "channel_id": 1,
    "post_id": 1
  }
}
Response:
type notify_post_response = {
  "version": "1.0",
  "id": 1,
}
ErrorCode:
	(ERR_ACCESS_TOKEN_EXP, "Access token invalid.")
	(ERR_NOT_AUTHORIZED, "Feeds service is not authorized.")
	(ERR_NOT_EXISTS, "Channel is not exists.")
	(ERR_INTERNAL_ERROR, "Feeds service internal error.")	
	(ERR_WRONG_STATE, "Post has been deleted.")
```



## Edit post

```YAML
Request:
type edit_post_request = {
  "version": "1.0"
  "method": "edit_post"
  "id": 1
  "params": {
    "access_token": "eyJhbGciOiAiRVMyNTYiLCAidHlwIjogIkpXVCIsICJ2ZXJzaW9uIjogIjEuMCIsICJraWQiOiAiZGlkOmVsYXN0b3M6aXFKd3NuUkJlNldSUUVhTVRKd1NhcU1idmI5NTJYOFZCUyNwcmltYXJ5In0.eyJpc3MiOiJkaWQ6ZWxhc3RvczppcUp3c25SQmU2V1JRRWFNVEp3U2FxTWJ2Yjk1Mlg4VkJTIiwiZXhwIjoxNjE0MTM0NjkwLCJhdWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwic3ViIjoiQWNjZXNzVG9rZW4iLCJuYW1lIjoidGVzdCIsInVzZXJEaWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwiZW1haWwiOiJOQSIsImFwcElkIjoiZGlkOmVsYXN0b3M6aXF0V1JWano3Z3NZaHl1UUViMWhZTk5tV1F0MVo5Z2VYZyIsImFwcEluc3RhbmNlRGlkIjoiZGlkOmVsYXN0b3M6aW1aZ0FvOVczOFZ6bzFwSlFmSHA2TkpwOUxac3JuUlBSciIsInVpZCI6MX0.vVk3WZ4jrBvSXssmCK6sVDJataJUMbpwg3jBPyJeRCJG4rnQUlcetXyh1EZwxdUzWAjsTA81-8eecp9CNgJbdQ",
    "channel_id": 1,
    "id": 1,
    "content": Any
  }
}
Response:
type edit_post_response = {
  "version": "1.0",
  "id": 1
}
ErrorCode:
	(ERR_ACCESS_TOKEN_EXP, "Access token invalid.")
	(ERR_NOT_AUTHORIZED, "Feeds service is not authorized.")
	(ERR_NOT_EXISTS, "Channel is not exists.")
	(ERR_INTERNAL_ERROR, "Feeds service internal error.")	
```



## Delete post

```YAML
Request:
type delete_post_request = {
  "version": "1.0"
  "method": "delete_post"
  "id": 1
  "params": {
    "access_token": "eyJhbGciOiAiRVMyNTYiLCAidHlwIjogIkpXVCIsICJ2ZXJzaW9uIjogIjEuMCIsICJraWQiOiAiZGlkOmVsYXN0b3M6aXFKd3NuUkJlNldSUUVhTVRKd1NhcU1idmI5NTJYOFZCUyNwcmltYXJ5In0.eyJpc3MiOiJkaWQ6ZWxhc3RvczppcUp3c25SQmU2V1JRRWFNVEp3U2FxTWJ2Yjk1Mlg4VkJTIiwiZXhwIjoxNjE0MTM0NjkwLCJhdWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwic3ViIjoiQWNjZXNzVG9rZW4iLCJuYW1lIjoidGVzdCIsInVzZXJEaWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwiZW1haWwiOiJOQSIsImFwcElkIjoiZGlkOmVsYXN0b3M6aXF0V1JWano3Z3NZaHl1UUViMWhZTk5tV1F0MVo5Z2VYZyIsImFwcEluc3RhbmNlRGlkIjoiZGlkOmVsYXN0b3M6aW1aZ0FvOVczOFZ6bzFwSlFmSHA2TkpwOUxac3JuUlBSciIsInVpZCI6MX0.vVk3WZ4jrBvSXssmCK6sVDJataJUMbpwg3jBPyJeRCJG4rnQUlcetXyh1EZwxdUzWAjsTA81-8eecp9CNgJbdQ",
    "channel_id": 1,
    "id": 1
  }
}
Response:
type delete_post_response = {
  "version": "1.0",
  "id": 1,
  "result":
}
ErrorCode:
	(ERR_ACCESS_TOKEN_EXP, "Access token invalid.")
	(ERR_NOT_AUTHORIZED, "Feeds service is not authorized.")
	(ERR_NOT_EXISTS, "Channel is not exists.")
	(ERR_INTERNAL_ERROR, "Feeds service internal error.")	
```



## Post comment

```YAML
Request:
type post_comment_request = {
  "version": "1.0"
  "method": "post_comment"
  "id": 1
  "params": {
    "access_token": "eyJhbGciOiAiRVMyNTYiLCAidHlwIjogIkpXVCIsICJ2ZXJzaW9uIjogIjEuMCIsICJraWQiOiAiZGlkOmVsYXN0b3M6aXFKd3NuUkJlNldSUUVhTVRKd1NhcU1idmI5NTJYOFZCUyNwcmltYXJ5In0.eyJpc3MiOiJkaWQ6ZWxhc3RvczppcUp3c25SQmU2V1JRRWFNVEp3U2FxTWJ2Yjk1Mlg4VkJTIiwiZXhwIjoxNjE0MTM0NjkwLCJhdWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwic3ViIjoiQWNjZXNzVG9rZW4iLCJuYW1lIjoidGVzdCIsInVzZXJEaWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwiZW1haWwiOiJOQSIsImFwcElkIjoiZGlkOmVsYXN0b3M6aXF0V1JWano3Z3NZaHl1UUViMWhZTk5tV1F0MVo5Z2VYZyIsImFwcEluc3RhbmNlRGlkIjoiZGlkOmVsYXN0b3M6aW1aZ0FvOVczOFZ6bzFwSlFmSHA2TkpwOUxac3JuUlBSciIsInVpZCI6MX0.vVk3WZ4jrBvSXssmCK6sVDJataJUMbpwg3jBPyJeRCJG4rnQUlcetXyh1EZwxdUzWAjsTA81-8eecp9CNgJbdQ",
    "channel_id": 1,
    "post_id": 1,
    "comment_id": 1,
    "content": Any
  }
}
Response:
type post_comment_response = {
  "version": "1.0",
  "id": 1,
  "result": {
    "id": 95
  }
}
ErrorCode:
	(ERR_ACCESS_TOKEN_EXP, "Access token invalid.")
	(ERR_NOT_EXISTS, "Channel or post is not exists.")
	(ERR_INTERNAL_ERROR, "Feeds service internal error.")	
```



## Edit comment

```YAML
Request:
type edit_comment_request = {
  "version": "1.0"
  "method": "edit_comment"
  "id": 1
  "params": {
    "access_token": "eyJhbGciOiAiRVMyNTYiLCAidHlwIjogIkpXVCIsICJ2ZXJzaW9uIjogIjEuMCIsICJraWQiOiAiZGlkOmVsYXN0b3M6aXFKd3NuUkJlNldSUUVhTVRKd1NhcU1idmI5NTJYOFZCUyNwcmltYXJ5In0.eyJpc3MiOiJkaWQ6ZWxhc3RvczppcUp3c25SQmU2V1JRRWFNVEp3U2FxTWJ2Yjk1Mlg4VkJTIiwiZXhwIjoxNjE0MTM0NjkwLCJhdWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwic3ViIjoiQWNjZXNzVG9rZW4iLCJuYW1lIjoidGVzdCIsInVzZXJEaWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwiZW1haWwiOiJOQSIsImFwcElkIjoiZGlkOmVsYXN0b3M6aXF0V1JWano3Z3NZaHl1UUViMWhZTk5tV1F0MVo5Z2VYZyIsImFwcEluc3RhbmNlRGlkIjoiZGlkOmVsYXN0b3M6aW1aZ0FvOVczOFZ6bzFwSlFmSHA2TkpwOUxac3JuUlBSciIsInVpZCI6MX0.vVk3WZ4jrBvSXssmCK6sVDJataJUMbpwg3jBPyJeRCJG4rnQUlcetXyh1EZwxdUzWAjsTA81-8eecp9CNgJbdQ",
    "channel_id": 1,
    "post_id": 1,
    "id": 1,
    "comment_id": 0,
    "content": Any
  }
}
Response:
type edit_comment_response = {
  "version": "1.0",
  "id": 1
}
ErrorCode:
	(ERR_ACCESS_TOKEN_EXP, "Access token invalid.")
	(ERR_NOT_AUTHORIZED, "Feeds service is not authorized.")
	(ERR_NOT_EXISTS, "Channel or post is not exists.")
	(ERR_INTERNAL_ERROR, "Feeds service internal error.")	
```



## Delete comment

```YAML
Request:
type delete_comment_request = {
  "version": "1.0"
  "method": "delete_comment"
  "id": 1
  "params": {
    "access_token": "eyJhbGciOiAiRVMyNTYiLCAidHlwIjogIkpXVCIsICJ2ZXJzaW9uIjogIjEuMCIsICJraWQiOiAiZGlkOmVsYXN0b3M6aXFKd3NuUkJlNldSUUVhTVRKd1NhcU1idmI5NTJYOFZCUyNwcmltYXJ5In0.eyJpc3MiOiJkaWQ6ZWxhc3RvczppcUp3c25SQmU2V1JRRWFNVEp3U2FxTWJ2Yjk1Mlg4VkJTIiwiZXhwIjoxNjE0MTM0NjkwLCJhdWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwic3ViIjoiQWNjZXNzVG9rZW4iLCJuYW1lIjoidGVzdCIsInVzZXJEaWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwiZW1haWwiOiJOQSIsImFwcElkIjoiZGlkOmVsYXN0b3M6aXF0V1JWano3Z3NZaHl1UUViMWhZTk5tV1F0MVo5Z2VYZyIsImFwcEluc3RhbmNlRGlkIjoiZGlkOmVsYXN0b3M6aW1aZ0FvOVczOFZ6bzFwSlFmSHA2TkpwOUxac3JuUlBSciIsInVpZCI6MX0.vVk3WZ4jrBvSXssmCK6sVDJataJUMbpwg3jBPyJeRCJG4rnQUlcetXyh1EZwxdUzWAjsTA81-8eecp9CNgJbdQ",
    "channel_id": 1,
    "post_id": 1,
    "id": 1
  }
}
Response:
type delete_comment_response = {
  "version": "1.0",
  "id": 1
}
ErrorCode:
	(ERR_ACCESS_TOKEN_EXP, "Access token invalid.")
	(ERR_NOT_AUTHORIZED, "Feeds service is not authorized.")
	(ERR_NOT_EXISTS, "Channel or post is not exists.")
	(ERR_INTERNAL_ERROR, "Feeds service internal error.")	
```



## Post like

```YAML
Request:
type post_like_request = {
  "version": "1.0"
  "method": "post_like"
  "id": 1
  "params": {
    "access_token": "eyJhbGciOiAiRVMyNTYiLCAidHlwIjogIkpXVCIsICJ2ZXJzaW9uIjogIjEuMCIsICJraWQiOiAiZGlkOmVsYXN0b3M6aXFKd3NuUkJlNldSUUVhTVRKd1NhcU1idmI5NTJYOFZCUyNwcmltYXJ5In0.eyJpc3MiOiJkaWQ6ZWxhc3RvczppcUp3c25SQmU2V1JRRWFNVEp3U2FxTWJ2Yjk1Mlg4VkJTIiwiZXhwIjoxNjE0MTM0NjkwLCJhdWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwic3ViIjoiQWNjZXNzVG9rZW4iLCJuYW1lIjoidGVzdCIsInVzZXJEaWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwiZW1haWwiOiJOQSIsImFwcElkIjoiZGlkOmVsYXN0b3M6aXF0V1JWano3Z3NZaHl1UUViMWhZTk5tV1F0MVo5Z2VYZyIsImFwcEluc3RhbmNlRGlkIjoiZGlkOmVsYXN0b3M6aW1aZ0FvOVczOFZ6bzFwSlFmSHA2TkpwOUxac3JuUlBSciIsInVpZCI6MX0.vVk3WZ4jrBvSXssmCK6sVDJataJUMbpwg3jBPyJeRCJG4rnQUlcetXyh1EZwxdUzWAjsTA81-8eecp9CNgJbdQ",
    "channel_id": 1,
    "post_id": 1,
    "comment_id": 1
  }
}
Response:
type post_like_response = {
  "version": "1.0",
  "id": 1
}
ErrorCode:
	(ERR_ACCESS_TOKEN_EXP, "Access token invalid.")
	(ERR_NOT_AUTHORIZED, "Feeds service is not authorized.")
  (ERR_WRONG_STATE, "Feeds service is in wrong state")
	(ERR_NOT_EXISTS, "Channel, post or comment is not exists.")
	(ERR_INTERNAL_ERROR, "Feeds service internal error.")	
```



## Post unlike

```YAML
Request:
type post_unlike_request = {
  "version": "1.0"
  "method": "post_unlike"
  "id": 1
  "params": {
    "access_token": "eyJhbGciOiAiRVMyNTYiLCAidHlwIjogIkpXVCIsICJ2ZXJzaW9uIjogIjEuMCIsICJraWQiOiAiZGlkOmVsYXN0b3M6aXFKd3NuUkJlNldSUUVhTVRKd1NhcU1idmI5NTJYOFZCUyNwcmltYXJ5In0.eyJpc3MiOiJkaWQ6ZWxhc3RvczppcUp3c25SQmU2V1JRRWFNVEp3U2FxTWJ2Yjk1Mlg4VkJTIiwiZXhwIjoxNjE0MTM0NjkwLCJhdWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwic3ViIjoiQWNjZXNzVG9rZW4iLCJuYW1lIjoidGVzdCIsInVzZXJEaWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwiZW1haWwiOiJOQSIsImFwcElkIjoiZGlkOmVsYXN0b3M6aXF0V1JWano3Z3NZaHl1UUViMWhZTk5tV1F0MVo5Z2VYZyIsImFwcEluc3RhbmNlRGlkIjoiZGlkOmVsYXN0b3M6aW1aZ0FvOVczOFZ6bzFwSlFmSHA2TkpwOUxac3JuUlBSciIsInVpZCI6MX0.vVk3WZ4jrBvSXssmCK6sVDJataJUMbpwg3jBPyJeRCJG4rnQUlcetXyh1EZwxdUzWAjsTA81-8eecp9CNgJbdQ",
    "channel_id": 1,
    "post_id": 1,
    "comment_id": 1
  }
}
Response:
type post_unlike_response = {
  "version": "1.0",
  "id": 1
}
ErrorCode:
	(ERR_ACCESS_TOKEN_EXP, "Access token invalid.")
	(ERR_NOT_AUTHORIZED, "Feeds service is not authorized.")
  (ERR_WRONG_STATE, "Feeds service is in wrong state")
	(ERR_NOT_EXISTS, "Channel, post or comment is not exists.")
	(ERR_INTERNAL_ERROR, "Feeds service internal error.")	
```



## Get my channels

```YAML
Request:
type get_my_channels_request = {
  "version": "1.0"
  "method": "get_my_channels"
  "id": 1
  "params": {
  "access_token": "eyJhbGciOiAiRVMyNTYiLCAidHlwIjogIkpXVCIsICJ2ZXJzaW9uIjogIjEuMCIsICJraWQiOiAiZGlkOmVsYXN0b3M6aXFKd3NuUkJlNldSUUVhTVRKd1NhcU1idmI5NTJYOFZCUyNwcmltYXJ5In0.eyJpc3MiOiJkaWQ6ZWxhc3RvczppcUp3c25SQmU2V1JRRWFNVEp3U2FxTWJ2Yjk1Mlg4VkJTIiwiZXhwIjoxNjA5NjY0MDA4LCJhdWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwic3ViIjoiQWNjZXNzVG9rZW4iLCJuYW1lIjoidGVzdCIsInVzZXJEaWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwiZW1haWwiOiJOQSIsImFwcElkIjoiZGlkOmVsYXN0b3M6dG9kbyIsImFwcEluc3RhbmNlRGlkIjoiZGlkOmVsYXN0b3M6aW1aZ0FvOVczOFZ6bzFwSlFmSHA2TkpwOUxac3JuUlBSciIsInVpZCI6MX0.nQEvzDeDx2aVCZhLmo-MuaHL3uxSwVgNfawHiPEsms9uU80Me2r86Wu1PP-gKbY6V6BxSs8naa4lgfUQxhRkEA",
  "by": 2, // 1:id, 2:last_update, 3:created_at
  "upper_bound": 0,
  "lower_bound": 0,
  "max_count": 0
}
}
Response:
type get_my_channels_response = {
  "version": "1.0",
  "id": 1,
  "result": {
    "is_last": false,
    "channels": [
      {
        "channel_id": 4,
        "name": "test18",
        "introduction": "test",
        "subscribers": 2,
        "avatar": Any
      },
      ......
      {
        "channel_id": 3,
        "name": "test16",
        "introduction": "12345",
        "subscribers": 1,
        "avatar": Any
      }
    ]
  }
}
ErrorCode:
	(ERR_ACCESS_TOKEN_EXP, "Access token invalid.")
	(ERR_NOT_AUTHORIZED, "Feeds service is not authorized.")
  (ERR_INTERNAL_ERROR, "Feeds service internal error.")	
```



## Get channels

```YAML
Request:
type get_channels_request = {
  "version": "1.0"
  "method": "get_channels"
  "id": 1
  "params": {
    "access_token": "eyJhbGciOiAiRVMyNTYiLCAidHlwIjogIkpXVCIsICJ2ZXJzaW9uIjogIjEuMCIsICJraWQiOiAiZGlkOmVsYXN0b3M6aXFKd3NuUkJlNldSUUVhTVRKd1NhcU1idmI5NTJYOFZCUyNwcmltYXJ5In0.eyJpc3MiOiJkaWQ6ZWxhc3RvczppcUp3c25SQmU2V1JRRWFNVEp3U2FxTWJ2Yjk1Mlg4VkJTIiwiZXhwIjoxNjA5NjY0MDA4LCJhdWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwic3ViIjoiQWNjZXNzVG9rZW4iLCJuYW1lIjoidGVzdCIsInVzZXJEaWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwiZW1haWwiOiJOQSIsImFwcElkIjoiZGlkOmVsYXN0b3M6dG9kbyIsImFwcEluc3RhbmNlRGlkIjoiZGlkOmVsYXN0b3M6aW1aZ0FvOVczOFZ6bzFwSlFmSHA2TkpwOUxac3JuUlBSciIsInVpZCI6MX0.nQEvzDeDx2aVCZhLmo-MuaHL3uxSwVgNfawHiPEsms9uU80Me2r86Wu1PP-gKbY6V6BxSs8naa4lgfUQxhRkEA",
    "by": 2, // 1:id, 2:last_update, 3:created_at
    "upper_bound": 0,
    "lower_bound": 0,
    "max_count": 0
  }
}
Response:
type get_channels_response = {
  "version": "1.0",
  "id": 1,
  "result": {
    "is_last": false,
    "channels": [
      {
        "id": 1,
        "name": "test12",
        "introduction": "test12345678",
        "owner_name": "test",
        "owner_did": "did:elastos:imZgAo9W38Vzo1pJQfHp6NJp9LZsrnRPRr",
        "subscribers": 1,
        "last_update": 1605495615,
        "avatar": Any
      },
      ......
      {
        "channel_id": 3,
        "name": "test16",
        "introduction": "12345",
        "owner_name": "test",
        "owner_did": "did:elastos:imZgAo9W38Vzo1pJQfHp6NJp9LZsrnRPRr",
        "subscribers": 1,
        "last_update": 1605495615,
        "avatar": Any
      }
    ]
  }
}
ErrorCode:
	(ERR_ACCESS_TOKEN_EXP, "Access token invalid.")
	(ERR_NOT_AUTHORIZED, "Feeds service is not authorized.")
  (ERR_INTERNAL_ERROR, "Feeds service internal error.")	
```



## Get subscribed channels

```YAML
Request:
type get_subscribed_channels_request = {
  "version": "1.0"
  "method": "get_subscribed_channels"
  "id": 1
  "params": {
    "access_token": "eyJhbGciOiAiRVMyNTYiLCAidHlwIjogIkpXVCIsICJ2ZXJzaW9uIjogIjEuMCIsICJraWQiOiAiZGlkOmVsYXN0b3M6aXFKd3NuUkJlNldSUUVhTVRKd1NhcU1idmI5NTJYOFZCUyNwcmltYXJ5In0.eyJpc3MiOiJkaWQ6ZWxhc3RvczppcUp3c25SQmU2V1JRRWFNVEp3U2FxTWJ2Yjk1Mlg4VkJTIiwiZXhwIjoxNjA5NjY0MDA4LCJhdWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwic3ViIjoiQWNjZXNzVG9rZW4iLCJuYW1lIjoidGVzdCIsInVzZXJEaWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwiZW1haWwiOiJOQSIsImFwcElkIjoiZGlkOmVsYXN0b3M6dG9kbyIsImFwcEluc3RhbmNlRGlkIjoiZGlkOmVsYXN0b3M6aW1aZ0FvOVczOFZ6bzFwSlFmSHA2TkpwOUxac3JuUlBSciIsInVpZCI6MX0.nQEvzDeDx2aVCZhLmo-MuaHL3uxSwVgNfawHiPEsms9uU80Me2r86Wu1PP-gKbY6V6BxSs8naa4lgfUQxhRkEA",
    "by": 2, // 1:id, 2:last_update, 3:created_at
    "upper_bound": 0,
    "lower_bound": 0,
    "max_count": 0
  }
}
Response:
type get_subscribed_channels_response = {
  "version": "1.0",
  "id": 1,
  "result": {
    "is_last": false,
    "channels": [
      {
        "id": 1,
        "name": "test12",
        "introduction": "test12345678",
        "owner_name": "test",
        "owner_did": "did:elastos:imZgAo9W38Vzo1pJQfHp6NJp9LZsrnRPRr",
        "subscribers": 1,
        "last_update": 1605495615,
        "avatar": Any
      },
      ......
      {
        "channel_id": 3,
        "name": "test16",
        "introduction": "12345",
        "owner_name": "test",
        "owner_did": "did:elastos:imZgAo9W38Vzo1pJQfHp6NJp9LZsrnRPRr",
        "subscribers": 1,
        "last_update": 1605495615,
        "avatar": Any
      }
    ]
  }
}
ErrorCode:
	(ERR_ACCESS_TOKEN_EXP, "Access token invalid.")
	(ERR_NOT_AUTHORIZED, "Feeds service is not authorized.")
  (ERR_INTERNAL_ERROR, "Feeds service internal error.")	
```



## Get posts

```YAML
Request:
type get_posts_request = {
  "version": "1.0"
  "method": "get_posts"
  "id": 1
  "params": {
    "access_token": "eyJhbGciOiAiRVMyNTYiLCAidHlwIjogIkpXVCIsICJ2ZXJzaW9uIjogIjEuMCIsICJraWQiOiAiZGlkOmVsYXN0b3M6aXFKd3NuUkJlNldSUUVhTVRKd1NhcU1idmI5NTJYOFZCUyNwcmltYXJ5In0.eyJpc3MiOiJkaWQ6ZWxhc3RvczppcUp3c25SQmU2V1JRRWFNVEp3U2FxTWJ2Yjk1Mlg4VkJTIiwiZXhwIjoxNjA5NjY0MDA4LCJhdWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwic3ViIjoiQWNjZXNzVG9rZW4iLCJuYW1lIjoidGVzdCIsInVzZXJEaWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwiZW1haWwiOiJOQSIsImFwcElkIjoiZGlkOmVsYXN0b3M6dG9kbyIsImFwcEluc3RhbmNlRGlkIjoiZGlkOmVsYXN0b3M6aW1aZ0FvOVczOFZ6bzFwSlFmSHA2TkpwOUxac3JuUlBSciIsInVpZCI6MX0.nQEvzDeDx2aVCZhLmo-MuaHL3uxSwVgNfawHiPEsms9uU80Me2r86Wu1PP-gKbY6V6BxSs8naa4lgfUQxhRkEA",
    "by": 2, // 1:id, 2:last_update, 3:created_at
    "channel_id": 1
    "upper_bound": 0,
    "lower_bound": 0,
    "max_count": 0
  }
}
Response:
type get_posts_response = {
  "version": "1.0",
  "id": 1,
  "result": {
    "is_last": false,
    "posts": [
      {
        "channel_id": 1,
        "id": 2,
        "status": "available",
        "comments": 0,
        "likes": 0,
        "created_at": 1605496137,
        "updated_at": 1605496137,
        "content": Any
      },
      ......
      {
        "channel_id": 1,
        "id": 3,
        "status": "available",
        "comments": 0,
        "likes": 0,
        "created_at": 1605496137,
        "updated_at": 1605496137,
        "content": Any
      }
    ]
  }
}
ErrorCode:
	(ERR_ACCESS_TOKEN_EXP, "Access token invalid.")
	(ERR_NOT_AUTHORIZED, "Feeds service is not authorized.")
	(ERR_NOT_EXISTS, "Channel or post is not exists.")
	(ERR_INTERNAL_ERROR, "Feeds service internal error.")	
```



## Get comments

```YAML
Request:
type get_comments_request = {
  "version": "1.0"
  "method": "get_comments"
  "id": 1
  "params": {
    "access_token": "eyJhbGciOiAiRVMyNTYiLCAidHlwIjogIkpXVCIsICJ2ZXJzaW9uIjogIjEuMCIsICJraWQiOiAiZGlkOmVsYXN0b3M6aXFKd3NuUkJlNldSUUVhTVRKd1NhcU1idmI5NTJYOFZCUyNwcmltYXJ5In0.eyJpc3MiOiJkaWQ6ZWxhc3RvczppcUp3c25SQmU2V1JRRWFNVEp3U2FxTWJ2Yjk1Mlg4VkJTIiwiZXhwIjoxNjA5NjY0MDA4LCJhdWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwic3ViIjoiQWNjZXNzVG9rZW4iLCJuYW1lIjoidGVzdCIsInVzZXJEaWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwiZW1haWwiOiJOQSIsImFwcElkIjoiZGlkOmVsYXN0b3M6dG9kbyIsImFwcEluc3RhbmNlRGlkIjoiZGlkOmVsYXN0b3M6aW1aZ0FvOVczOFZ6bzFwSlFmSHA2TkpwOUxac3JuUlBSciIsInVpZCI6MX0.nQEvzDeDx2aVCZhLmo-MuaHL3uxSwVgNfawHiPEsms9uU80Me2r86Wu1PP-gKbY6V6BxSs8naa4lgfUQxhRkEA",
    "channel_id": 1
    "post_id": 1,
    "by": 2, // 1:id, 2:last_update, 3:created_at
    "upper_bound": 0,
    "lower_bound": 0,
    "max_count": 0
  }
}
Response:
type get_comments_response = {
  "version": "1.0",
  "id": 1,
  "result": {
    "is_last": false,
    "posts": [
      {
        "channel_id": 1,
        "post_id": 1
        "id": 2,
        "status": "available",
        "comment_id": 0
        "user_name": "user1"
        "content": Any
        "likes": 0,
        "created_at": 1605496137,
        "updated_at": 1605496137,
      },
      ......
       {
        "channel_id": 1,
        "post_id": 1
        "id": 3,
        "status": "available",
        "comment_id": 0
        "user_name": "user1"
        "content": Any
        "likes": 0,
        "created_at": 1605496137,
        "updated_at": 1605496137,
      }
    ]
  }
}
ErrorCode:
	(ERR_ACCESS_TOKEN_EXP, "Access token invalid.")
	(ERR_NOT_AUTHORIZED, "Feeds service is not authorized.")
	(ERR_NOT_EXISTS, "Channel or post is not exists.")
	(ERR_INTERNAL_ERROR, "Feeds service internal error.")	
```



## Get statistics

```YAML
Request:
type get_statistics_request = {
  "version": "1.0"
  "method": "get_statistics"
  "id": 1
  "params": {
    "access_token": "eyJhbGciOiAiRVMyNTYiLCAidHlwIjogIkpXVCIsICJ2ZXJzaW9uIjogIjEuMCIsICJraWQiOiAiZGlkOmVsYXN0b3M6aXFKd3NuUkJlNldSUUVhTVRKd1NhcU1idmI5NTJYOFZCUyNwcmltYXJ5In0.eyJpc3MiOiJkaWQ6ZWxhc3RvczppcUp3c25SQmU2V1JRRWFNVEp3U2FxTWJ2Yjk1Mlg4VkJTIiwiZXhwIjoxNjA5NjY0MDA4LCJhdWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwic3ViIjoiQWNjZXNzVG9rZW4iLCJuYW1lIjoidGVzdCIsInVzZXJEaWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwiZW1haWwiOiJOQSIsImFwcElkIjoiZGlkOmVsYXN0b3M6dG9kbyIsImFwcEluc3RhbmNlRGlkIjoiZGlkOmVsYXN0b3M6aW1aZ0FvOVczOFZ6bzFwSlFmSHA2TkpwOUxac3JuUlBSciIsInVpZCI6MX0.nQEvzDeDx2aVCZhLmo-MuaHL3uxSwVgNfawHiPEsms9uU80Me2r86Wu1PP-gKbY6V6BxSs8naa4lgfUQxhRkEA"
  }
}
Response:
type get_statistics_response = {
  "version": "1.0",
  "id": 1,
  "result": {
    "did": "did:elastos:imZgAo9W38Vzo1pJQfHp6NJp9LZsrnRPRr",
    "connecting_clients": 2,
    "total_clients": 3
  }
}
ErrorCode:
	(ERR_ACCESS_TOKEN_EXP, "Access token invalid.")
	(ERR_DB_ERROR, "Query db error.")
```



## Subscribe channel

```YAML
Request:
type subscribe_channel_request = {
  "version": "1.0"
  "method": "subscribe_channel"
  "id": 1
  "params": {
    "access_token": "eyJhbGciOiAiRVMyNTYiLCAidHlwIjogIkpXVCIsICJ2ZXJzaW9uIjogIjEuMCIsICJraWQiOiAiZGlkOmVsYXN0b3M6aXFKd3NuUkJlNldSUUVhTVRKd1NhcU1idmI5NTJYOFZCUyNwcmltYXJ5In0.eyJpc3MiOiJkaWQ6ZWxhc3RvczppcUp3c25SQmU2V1JRRWFNVEp3U2FxTWJ2Yjk1Mlg4VkJTIiwiZXhwIjoxNjA5NjY0MDA4LCJhdWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwic3ViIjoiQWNjZXNzVG9rZW4iLCJuYW1lIjoidGVzdCIsInVzZXJEaWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwiZW1haWwiOiJOQSIsImFwcElkIjoiZGlkOmVsYXN0b3M6dG9kbyIsImFwcEluc3RhbmNlRGlkIjoiZGlkOmVsYXN0b3M6aW1aZ0FvOVczOFZ6bzFwSlFmSHA2TkpwOUxac3JuUlBSciIsInVpZCI6MX0.nQEvzDeDx2aVCZhLmo-MuaHL3uxSwVgNfawHiPEsms9uU80Me2r86Wu1PP-gKbY6V6BxSs8naa4lgfUQxhRkEA",
    "id": 1
  }
}
Response:
type subscribe_channel_response = {
  "version": "1.0",
  "id": 1
}
ErrorCode:
	(ERR_ACCESS_TOKEN_EXP, "Access token invalid.")
	(ERR_WRONG_STATE, "Feeds service is wrong state to issue credential.")
	(ERR_NOT_EXISTS, "Channel is not exists.")
	(ERR_INTERNAL_ERROR, "Feeds service internal error.")
```



## Unsubscribe_channel

```YAML
Request:
type unsubscribe_channel_request = {
  "version": "1.0"
  "method": "unsubscribe_channel"
  "id": 1
  "params": {
    "access_token": "eyJhbGciOiAiRVMyNTYiLCAidHlwIjogIkpXVCIsICJ2ZXJzaW9uIjogIjEuMCIsICJraWQiOiAiZGlkOmVsYXN0b3M6aXFKd3NuUkJlNldSUUVhTVRKd1NhcU1idmI5NTJYOFZCUyNwcmltYXJ5In0.eyJpc3MiOiJkaWQ6ZWxhc3RvczppcUp3c25SQmU2V1JRRWFNVEp3U2FxTWJ2Yjk1Mlg4VkJTIiwiZXhwIjoxNjA5NjY0MDA4LCJhdWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwic3ViIjoiQWNjZXNzVG9rZW4iLCJuYW1lIjoidGVzdCIsInVzZXJEaWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwiZW1haWwiOiJOQSIsImFwcElkIjoiZGlkOmVsYXN0b3M6dG9kbyIsImFwcEluc3RhbmNlRGlkIjoiZGlkOmVsYXN0b3M6aW1aZ0FvOVczOFZ6bzFwSlFmSHA2TkpwOUxac3JuUlBSciIsInVpZCI6MX0.nQEvzDeDx2aVCZhLmo-MuaHL3uxSwVgNfawHiPEsms9uU80Me2r86Wu1PP-gKbY6V6BxSs8naa4lgfUQxhRkEA",
    "id": 1
  }
}
Response:
type unsubscribe_channel_response = {
  "version": "1.0",
  "id": 1,
  "result":
}
ErrorCode:
	(ERR_ACCESS_TOKEN_EXP, "Access token invalid.")
	(ERR_WRONG_STATE, "Feeds service is wrong state to issue credential.")
	(ERR_NOT_EXISTS, "Channel is not exists.")
	(ERR_INTERNAL_ERROR, "Feeds service internal error.")
```



## Enable notification

```YAML
Request:
type enable_notification_request = {
  "version": "1.0"
  "method": "enable_notification"
  "id": 1
  "params": {
 	  "access_token": "eyJhbGciOiAiRVMyNTYiLCAidHlwIjogIkpXVCIsICJ2ZXJzaW9uIjogIjEuMCIsICJraWQiOiAiZGlkOmVsYXN0b3M6aXFKd3NuUkJlNldSUUVhTVRKd1NhcU1idmI5NTJYOFZCUyNwcmltYXJ5In0.eyJpc3MiOiJkaWQ6ZWxhc3RvczppcUp3c25SQmU2V1JRRWFNVEp3U2FxTWJ2Yjk1Mlg4VkJTIiwiZXhwIjoxNjA5NjY0MDA4LCJhdWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwic3ViIjoiQWNjZXNzVG9rZW4iLCJuYW1lIjoidGVzdCIsInVzZXJEaWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwiZW1haWwiOiJOQSIsImFwcElkIjoiZGlkOmVsYXN0b3M6dG9kbyIsImFwcEluc3RhbmNlRGlkIjoiZGlkOmVsYXN0b3M6aW1aZ0FvOVczOFZ6bzFwSlFmSHA2TkpwOUxac3JuUlBSciIsInVpZCI6MX0.nQEvzDeDx2aVCZhLmo-MuaHL3uxSwVgNfawHiPEsms9uU80Me2r86Wu1PP-gKbY6V6BxSs8naa4lgfUQxhRkEA"
  }
}
Response:
type enable_notification_response = {
  "version": "1.0",
  "id": 1,
  "result":
}
ErrorCode:
	(ERR_ACCESS_TOKEN_EXP, "Access token invalid.")
	(ERR_INTERNAL_ERROR, "Feeds service internal error.")
```



## Get service version

```YAML
Request:
type get_service_version_request = {
  "version": "1.0"
  "method": "get_service_version"
  "id": 1
  "params": {
    "access_token": ""
  }
}
Response:
type get_service_version_response = {
  "version": "1.0",
  "id": 1,
  "result": {
    "version": "1.5.0(cd4bf70)",
    "version_code": 10500
  }
}
ErrorCode:
```

