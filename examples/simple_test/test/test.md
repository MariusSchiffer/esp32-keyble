keyble = require("/usr/lib/node_modules/keyble/keyble.js"); a = new keyble.Key_Ble({address:"00:1a:22:09:8a:64", user_id:2, user_key: "7e9c1ac0c448189233a6e5278318e996"}); a.test("2DCF462904B478D8","02C97296F93CC522"); a.unlock()

[ 126, 156, 26, 192, 196, 72, 24, 146, 51, 166, 229, 39, 131, 24, 233, 150 ]

data : [1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16]

encrypted: [ 44, 51, 43, 5, 118, 129, 80, 47, 208, 223, 124, 87, 153, 24, 157, 29 ]


808760436e0ca5a3185f0001ae918ac5
808760436E0CA5A3185F0001AE918AC5


Mögliche Ursachen:
Local Session nonce falsch geschickt (definitiv richtig)
Remote nonce falsch geparst
Es wird nicht exakt die Nachricht wie angezeigt gesendet


tcpdump:

81040325878b16c01fecdecf07c3e6c8
8104035682708FA8E38F3CFA698026EF


registering:

Registering user on Smart Lock with address "00:1a:22:09:8a:64", card key "89e71cfd4a6395b5d47e0672bb0572a6" and serial "NEQ1057542"...
8002ff44957794070a6ee20000000000
received:
80030393cae0fbdaeb09240010170000
crypt data input: 88 39 2b db 24 86 cf 7f 9f 11 23 3e d9 7d 47 94
compute nonce in crypt data: 04 93 ca e0 fb da eb 09 24 00 00 00 01
crypt data output: 25 87 8b 16 c0 1f ec de cf 07 c3 e6 c8 b4 a8 cf
compute auth value input: 03 88 39 2b db 24 86 cf 7f 9f 11 23 3e d9 7d 47 94 00 00 00 00 00 00
nonce in auth value: 04 93 ca e0 fb da eb 09 24 00 00 00 01
auth value output data:  5d 51 4e 88
sent:
81040325878b16c01fecdecf07c3e6c8
received:
80008100000000000000000000000000
sent:
00b4a8cf00000000000000015d514e88
received:
80815b54f93d515812b5000128152c6b
crypt data input: 5b 54 f9 3d 51 58 12 b5
compute nonce in crypt data: 81 44 95 77 94 07 0a 6e e2 00 00 00 01
crypt data output: 00 00 00 00 00 00 00 00
compute auth value input: 00 00 00 00 00 00 00 00
nonce in auth value: 81 44 95 77 94 07 0a 6e e2 00 00 00 01
auth value output data:  28 15 2c 6b
User registered. Use arguments: --address 00:1a:22:09:8a:64 --user_id 3 --user_key 88392bdb2486cf7f9f11233ed97d4794


registering esp:
sending actual fragment:
8002FF8AD54D2B959FBCF00000000000
message notify, id:
8003039ECE607747CC982C0010170000
got message fragment
got last fragment
Remote session nonce, user id
9ECE607747CC982C
3
nonces are exchanged
emitting event:
nonces-exchanged
4
crypt_data, input data
4E9B1BCBCB1B1B9190ABEB2B8B08EB90
compute nonce in crypt data:
049ECE607747CC982C00000001
nonce length:
13
16
crypt_data, output data
14B1B20922D5FA92DEF7E0C77B7AA163
computing auth value, input data
034E9B1BCBCB1B1B9190ABEB2B8B08EB900000000000
compute nonce in auth value:
049ECE607747CC982C00000001
auth value, output data
D58F0BDD
unsecured message, preparing
fragment pushed to queue
fragment pushed to queue
queue size:
2
sending actual fragment:
81040314B1B20922D5FA92DEF7E0C77B
message notify, id:
80008100000000000000000000000000
got message fragment
got last fragment
got ack, do send next
sending actual fragment:
007AA1630000000000000001D58F0BDD
message notify, id:
80008100000000000000000000000000