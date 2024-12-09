#include <cassert>
#include "fenboard.hh"
#include "move.hh"

const uint64_t zobrist_hashes[] = {0xe02111c7659eae86L, 0x6da4e7350d2011b3L, 0x83cd50fcc552e5e9L, 0x78615f6439c6c6b4L, 0x51c92cc23497add2L,
0xc8fc6be58ad5a05fL, 0xd5e910e9d62f114dL, 0x7ef0daa1916a9daaL, 0xc6aac4139c2cc032L, 0x9003d09f97066fc5L, 0x6a78933e52670af4L, 0x6ad0ad6da26c3d19L,
0xefdc626850a3e9d9L, 0x23fbf0da01209946L, 0xff26e2a098ce09c0L, 0x95199b3215da66b0L, 0x7d6367bdd38c52faL, 0xf17fe7c7cf19d2b4L, 0x4837f5aba116f8a2L,
0x4f66830006502645L, 0xa4f58972cc89f3fcL, 0xdea8789b9dbbe477L, 0xad7b4393cd4bb600L, 0x6b10f5f748599647L, 0x85a319181ea35747L, 0x246fecd9109c5d52L,
0x168ed09595f7df38L, 0x8c6bba3aaaef340dL, 0xb40e45ed3369cb74L, 0xbde1a58739ec73L, 0x1b6e71b2ec8ce71eL, 0xadcb55dd6f0602fL, 0x24debf78ceb85ef6L,
0xed5067a033af109eL, 0x78f45cf85520a20eL, 0xbd4c56d2866b6ae1L, 0xf552502c3bdf332L, 0x4d08466a8cd4cc2L, 0xe8ee56ec12df978L, 0xcde413d7b5742c9L,
0xa64c9766137404b6L, 0x556e2a88ae578dbfL, 0x52e4fa59ca741895L, 0x84fd4c736ab37055L, 0x3d19378478a6004dL, 0x1efc8f3f6243b9efL, 0xeffcfbe872e27214L,
0xbb755410ad7518f4L, 0xe7687c398f501f7fL, 0xa9887346984cadf8L, 0xe7990a9b385b2fb1L, 0x18460e946b81774dL, 0xa3a9d9579eed6f2aL, 0xca6b9dfeb1ea3055L,
0xe981e73c0abdb693L, 0x9b6540d7e205f5L, 0x7023035da61ce9e8L, 0xba61e58796bb925fL, 0xb2519759680b076cL, 0x54d7165c0538a919L, 0x40653ffe5cc1940aL,
0x168f85affc87a4dbL, 0x486023b5ce1de6b2L, 0x413bd10a0e6775a8L, 0x4ceaab3e0186f2c3L, 0xe5431e2e2ffe953aL, 0x41f95e2d1f1bfb4bL, 0x1e35ec556e077c82L,
0xd09db63d56f72a8aL, 0x47eb45c372fc48cfL, 0xdab024345d3ef3a6L, 0x13bdba3d9ace3960L, 0xf11249f6d41b497L, 0xc02480cbc7f1066dL, 0x49bad8c786e36b5dL,
0xf028ee71487b0898L, 0x94c08209aaed2cbfL, 0x9cdda1876604163fL, 0xefee394ae0c73abL, 0xb0d8925119193e95L, 0xc1fac6ab7439df00L, 0x457b11d58de47b4cL,
0x2a6beb79a4d65462L, 0xb935574ce7253d05L, 0x7c880a4fe94f0adeL, 0x976918e8a8256b47L, 0x65751dc8eede76a6L, 0x9bb4ea5e00988851L, 0xdb6f0f8064d3e038L,
0xeeb896b7e4f148c1L, 0xccd1809cddb1ed91L, 0xf5b464f4b1e87bd7L, 0xe5ca8b119a28c4edL, 0x757fc2632e870eL, 0x25ea79e54c48e8e0L, 0x6c670f34b2499eb3L,
0xf272471309a57a74L, 0x58917386d789d83fL, 0x5f30a770efd0418fL, 0x7f8272e2fbab644fL, 0xa19a3efafc81ff9fL, 0x67686fe1a51d4219L, 0x768f99fe57436478L,
0x1a0550cd0e44ce6fL, 0xbb68cb6a763dd679L, 0x9f7ee195a353ea67L, 0xb8c3b743d0ee17cL, 0x68ee4cbc2f55001cL, 0xe710344acc6e063dL, 0x7e82d2727f6d00a3L,
0x1bec90d0f681f5ffL, 0x5e0fc6b9ff32e59eL, 0x32486f6467c74701L, 0xd5f922f18e167471L, 0x6611767b362e51deL, 0xdb736134ef606112L, 0x87e25b87d0b82117L,
0x592024d472e9b429L, 0xa843d541bd752ac4L, 0xc05fb377703e539eL, 0x2a842e03bfc34776L, 0xb6b731c087801de2L, 0x99836b1c5fb2052L, 0x86dda93f27aed1e3L,
0x689c3121f48b7b8dL, 0xf73f53b546e8a39fL, 0x1d129ae53308a565L, 0x18953add2057232bL, 0x50f59199d39dd3e2L, 0x792c499ab29e93f4L, 0x60a93caf5bc7357fL,
0x343d68828050e150L, 0x1221303626829d85L, 0x22317bccd7c156d9L, 0x941e3911603f810cL, 0xcc1d8cc38de647a1L, 0x5fbfc09833cfc3cdL, 0xda39dae0ede57888L,
0x1292c203e4b38826L, 0xb5f71770ae190a2eL, 0xe52a80282c3b626fL, 0x4907a1847a0a48f3L, 0x61ca184e64535f5dL, 0x7100abda1d4085a7L, 0x866fbbbab5984ec2L,
0xc067f5b4fff25e03L, 0x1b929ecf1c676e69L, 0xd311e446dd475ab5L, 0x7c3f7a6bf2f4c8acL, 0x63237ee4afc1a442L, 0xbe2693a016500fd7L, 0x90272e0ae2754308L,
0xe653afae59824ed9L, 0xbaa37d9f2a61036fL, 0x3acafc5157d5448aL, 0xf281134f50edb7deL, 0x4d22c8e9a33c5aecL, 0xa859f92794c928e0L, 0xdd5d00367dd385fL,
0x1d85c7ea04d773bfL, 0xccd60ec1f904e1a5L, 0x7316211557b7b24dL, 0x6d415afdc956148aL, 0xd44f2ec0cde1572dL, 0x30670c219d61d1a5L, 0x367fbd71ad47ced2L,
0xe7db7fecea67fe66L, 0xa5ba9bd21bca6ec4L, 0x61d3c0dda9dc3a08L, 0xe15299640169ae61L, 0xdcb655c24672aa3cL, 0x2f0293295d6addc5L, 0x16abc74cd7f5f4a1L,
0xc5dac2f5bdc67e5aL, 0x3fcbdb2198c2325bL, 0xf48bddbe2cff0ff5L, 0x3a2efba78ff061a3L, 0xd501e98587ffaa3cL, 0x4a06edfad212be20L, 0x9564bfbe936d0fa2L,
0xfe41617621975198L, 0xf6cb5308b994f866L, 0x9dbbe6e2d6960a45L, 0xd9f7023ce3832732L, 0x9c118152aadde3f8L, 0xa84f7f54395888beL, 0xb2bf8b0ca355c7b1L,
0x235fb4f73cc324c0L, 0xcad2f1760863afe2L, 0x82800fcd02183730L, 0x8d53ed151f951371L, 0xa409404098a69e1eL, 0xe76100c2a082d6d6L, 0x37a7c41f64c1a3a2L,
0x197e5d366662847cL, 0x657b6de87f7e7077L, 0x4dc814cecd1de71cL, 0xe31d53ed02b6e609L, 0xc924124966da58eL, 0x4c8fe37a001e064dL, 0xb106e3d58e7a9422L,
0xe2e7be07c8c7b7d5L, 0xa22224ae3e6485d8L, 0xa7f8b9614605e8aaL, 0x287fd02f9dd3b869L, 0x95851c81e159a1beL, 0x8956eb855f29a48L, 0x16b1bf0275df9c77L,
0x47e6fd3c3f3b7ee7L, 0xd47444ea9f5361c0L, 0xb5e89f2705fe52bfL, 0x13f066d7143f5588L, 0xc527f89fa1eae7a3L, 0x736a86dca1cf1540L, 0x32dbd3a6ad38e1e9L,
0x90837f579ebb8692L, 0xfbd28e74ebfb05ecL, 0x5eaca369b54b2e74L, 0xcd57eb0e7aa455eL, 0xa2a8f37d5188388cL, 0x67d8d658c1c3158cL, 0xfb7d9f62e85c1009L,
0x70b935e0022fc77cL, 0xaa0296547be79928L, 0x64186aff1ebaa64cL, 0x5334b277fa707270L, 0x2df92a55f3c4a073L, 0x6d7c4e9d9ae27ef7L, 0xae6b6f3c76f4a9f9L,
0x5a26c0bd2179bf5cL, 0x6eb9b2d5b1d834c2L, 0x183181073cd500dbL, 0xfde621a0121df43cL, 0x5317bf4c4b0ef577L, 0x4a9111532cfdaeecL, 0x427796da74538fdbL,
0x9c9256da3c6966feL, 0x24d6fe3fb5b1f860L, 0x12b3dae6aec8c5aeL, 0xa11f8fefaa955139L, 0xb2e48a164a7ab6a6L, 0x2868e5c6cbec09e5L, 0xf3c1ba3649e91fe2L,
0x51fbf818941d1f23L, 0x96962e35a0229b63L, 0x1a57a5960c59ae63L, 0x80706e6ab2fed9d5L, 0x6233aaf4a771afd8L, 0x29381ec58d077487L, 0xf6243cab4958effbL,
0xb25a1f03d14e197eL, 0x4eb418b37c000af7L, 0x5abf427e4872a0dfL, 0x52841f7082d3d090L, 0x8e35bee31b5cd60aL, 0x533aefb34061c9deL, 0x6334817a4b97f03fL,
0xe1ee72a2dd4f2271L, 0x468e76ba573e459bL, 0xcfbb76e816a06069L, 0x45479d3aa5cbc520L, 0x229b6f77bf240787L, 0xd2852845d0d8f033L, 0x312b325f1a53cfbcL,
0xea85bce6ea768a32L, 0x9b113597204cc6f3L, 0x10d0aa892fe6d551L, 0xaac58a164d2dca5cL, 0x7040b3632375e9dfL, 0xa7cf808f3ab85af4L, 0x8224a9a91e53c2deL,
0xe25e2f7cadabf5d7L, 0x857e5e8ba67c5bd6L, 0xddba1fe0d16d4c22L, 0x93faaf7ae785bccL, 0xa1fe0ec2dfd7a2ffL, 0xe4f5caf7614e771eL, 0xe9023c53a3645784L,
0x322e9df7056442e5L, 0x9e0fff3980764f63L, 0xa03d32cb0729a731L, 0x3ad238c85db5172fL, 0xc6dc67ef0655062eL, 0x1205f3d49b09f008L, 0xa248a320e1e38a14L,
0x565947feff72e498L, 0x2cc2b57ba4673d95L, 0x82f74ef0627e0e8aL, 0x2a4dfbeb2d282003L, 0xd7c9cba6a75f9abaL, 0x8f1cf955e1dd2511L, 0x819a7d4b8ae7bdbeL,
0x5c00466aaad1e533L, 0x66be08ad13cb5422L, 0x4a4b47719e33fadcL, 0xb45ddd989ed26560L, 0xce41d914171c194L, 0xcbea2cb487748238L, 0xc87720748b4be85fL,
0xe171855a7cfeb71fL, 0x77c04e6b4dba374L, 0x68c9f3b015709cfaL, 0x28e389d655662e9fL, 0x67dea09358345d2aL, 0xd52be7639ec3e817L, 0xc7cbff6bc4237d01L,
0x3a28a85c97f20b00L, 0x53ca278b825630f2L, 0x2729410ed2863fa6L, 0xeaf9e5c0fe74b9b9L, 0xec1c701b73759bafL, 0xc6a3edbb27292bd0L, 0x8992a19c36bf05f5L,
0xbff14ae110acd925L, 0xdc66f3a4f472506dL, 0x54cbb74d404d78caL, 0xd1c2c87f30741ecfL, 0xa8b16475cdd8041cL, 0xc8dcc544c4d60b9bL, 0xc07a401e348678c1L,
0xd983bd0cea7a14a6L, 0x1c0e4a71639398f2L, 0x831f60904daa6cecL, 0xd39bf1e44f3611eeL, 0xf4b3f97ba3851f4eL, 0x4df4bf61b324062aL, 0x1f8b7adfe0cad1fbL,
0xa5c32f956d4cdd2eL, 0x9e3864512051a6fcL, 0xd6ad473be74c5608L, 0x7cfa74eafd51bbdbL, 0x262590fd3121b856L, 0x8062ed983774c45eL, 0x52363fbbc8e14b3bL,
0x922f0bac68ade882L, 0x79b0426954e22028L, 0xbe64279d6cd8e65aL, 0x136ca9b2515cbe18L, 0xf1f1380f29a259cL, 0x9c3679e1788b27f4L, 0xa4d3b61cfc566367L,
0xf33a5b3df3922853L, 0xfeefdf0072229b6aL, 0x28c8b6f207ff3132L, 0x6604ed049cc7dd6cL, 0x4425bbc7cfc4d559L, 0xf34553c57c30509eL, 0x77b48debbf27690dL,
0xd87e8c51144746ddL, 0x509633e137966ba8L, 0x4f25f94261e3077bL, 0xdeea61427f656fa0L, 0xe5cbf40678fa9696L, 0xf4e5ac9ebf4af583L, 0xb46abd905f09e033L,
0xbc814b1c97cc5cc0L, 0x63275ebfa5b70e39L, 0x781a8ef629eefc3bL, 0xb4767ac91432e441L, 0xfcc341f601afc8b1L, 0x17a831ef567feabL, 0xbe732f7e6089a6fcL,
0x60ffbbcadf21d268L, 0xe120fc9c889932b5L, 0xd5afd5a0c3ed74f6L, 0x6e290b8ede6dcf94L, 0xc88d8bcbe646103L, 0x844390c286f6e72L, 0x839701715a3c0764L,
0x2196cfa630fd61f9L, 0x9e435c31208a28f5L, 0xaabe53e52f4371d8L, 0x94badadfbb9649aL, 0x6b3fce74d9a35fd9L, 0x64ab6452f8fbab85L, 0x35c22c367b4da29aL,
0x46ad3aa1645daec2L, 0xc2e992942902009eL, 0x2c95c926602d58dbL, 0xd4015b291f462348L, 0x6ada11c6ade62010L, 0xa5cab39455b2301fL, 0xb0831359121f846dL,
0x4cfdfed7e6361c5L, 0x2956dfbff81f6cfaL, 0x4f56a9cd47603be0L, 0x13a7eda86ec3b4f0L, 0x2501e0c2cee590fbL, 0xeece6bbbcdba15a3L, 0x6d88ee8202dbe0a8L,
0x7799508e8d54ad78L, 0x644219e2442ac442L, 0x63daf707b860c962L, 0xc0cb22a440582e24L, 0x100a42cc8ee935c1L, 0xc5fde50b1290efabL, 0x90ae541606075f77L,
0xc844b757dcdad199L, 0x2a34d306e70dec6dL, 0xefb6a23ad93e143cL, 0x26eb34c92e1886c7L, 0x9b5a78a2a34027cL, 0x2c0bd34e98ebdd64L, 0x327534f23278d685L,
0xeead60fa6e49821fL, 0x2dae76f316d4266bL, 0xc55e80c759ffb5d9L, 0xc492d56350f1aa5eL, 0x8851b7fb384161feL, 0xada5230348f95383L, 0xfcddb3b09f6e2c8fL,
0xe3015e14ed74aaa2L, 0xa336277bb9b6565cL, 0x39be7efcf6d80e75L, 0x17a810034fe55d57L, 0xe5fad285c8f38a5aL, 0xcd0ae0f936f5c891L, 0xe6e04ade4d2c4b20L,
0xca0130280d85a234L, 0x6b0617ec9749f729L, 0x64844c91522bc90eL, 0x2551aa8cbd0cb230L, 0x9a7af49e60af5bc0L, 0x495f4269368be4b4L, 0xf048c3a743deb60eL,
0x14bf98f0d7cf70f0L, 0x715c82bbe10f77edL, 0x510192977d399061L, 0xaf1f5aac7fc7fdf1L, 0x4c094267f9c4756aL, 0xcbadd93b1b098b62L, 0x431bd04788585e70L,
0x2c6e3007391f3b7bL, 0xd38d628a49efa311L, 0x2ce85221e8cda8c6L, 0xfbcff3faa7568279L, 0x4347aee7fc96290L, 0x2433b3d9b6cc48d3L, 0x2bfec561ce058010L,
0x702d6aaf2a48f533L, 0x907a594309caa4b4L, 0xdc8ec3baa1e6d877L, 0x159a402b1730ff1aL, 0xa91fdd7c3ddb47f3L, 0x8783f5c4565d0620L, 0x16c6bb0809049f39L,
0x4cccaabc1e34cb46L, 0xd8e512e7fe4980b6L, 0xe41f11dfb6f778e2L, 0xa22b5802918e7ec4L, 0xde7ca8efb0a24f8eL, 0xcbc8af2cb8c22a20L, 0x9b40e135f6548b02L,
0x847686607b3e7eadL, 0xd10c337a138ee1c6L, 0xe5c7a2db8b491d11L, 0xf80695e8e8e6179fL, 0xb548af291a24611aL, 0x8f3106516288a076L, 0xf55b4ac53d6b5aaL,
0xc4d55067aa0817a9L, 0x70debea86a52e853L, 0x3f30068ec8e06d62L, 0xbc70fabab8db83fbL, 0x65c5c9e60ba76b1aL, 0x2eaf5d6b2093c905L, 0x1ba3d2f7a75a1e4bL,
0x47cee6cb54411833L, 0xc4867d4476327bb2L, 0x240dff3933e709d3L, 0x16ca25f60d751d29L, 0xe88c6a9989541accL, 0xfef71918de26f506L, 0x5872b502d1706814L,
0x578a07d6d7aacd81L, 0x83a8cfcc7726dc91L, 0xcad547c51524a4ebL, 0x9b1aff51e47d0e5fL, 0xe07d4642413453e6L, 0x5dbd75a15a7ded63L, 0xd9e18fb5591d7e28L,
0x817fa3791c15c224L, 0x4da0c2fe8134b98eL, 0x1f0f85b48d68aec6L, 0xf30e4e1068194743L, 0xc2c4fc7eca79c219L, 0x1b1d9527732449e2L, 0x9dc3e308aceeeef7L,
0xbc3cb6f1dc0dd1feL, 0x86c3257f1caedd75L, 0x5a9c43815d61d9d8L, 0x248d80d5a1e11db8L, 0x1829a02a44bf95eL, 0xf76947d67be14811L, 0x841fd84e0b1fb4bdL,
0x899228f52fd93036L, 0x821763afe12bcf3eL, 0xea12ff65048146b4L, 0x43f4f086da963187L, 0xced27366883b67dbL, 0x7b77b8f3c2b7ef5cL, 0x5e410fc30a4a3030L,
0x48a0e20885e9643aL, 0x129611972701aef8L, 0x3b71f5a9073fbc54L, 0xca9829c7e16369b2L, 0x741178a4933b84e7L, 0xbdbc812b5c0702efL, 0xf15d7c579030954aL,
0x4dd39973f8abb5ccL, 0x5342d0d383297c34L, 0x93390567cf51ac43L, 0x42cff0c1275ef473L, 0xe96673f0e1d5b51L, 0xa908550bf3ddfb82L, 0x766691414984420cL,
0xb3ab0dde32a46044L, 0x999105a9f3d89303L, 0xa89761c7acda2976L, 0x8f880af64ba51ff1L, 0x6684eb8e828aba97L, 0x4d9b22d4381c7a7dL, 0x44e7218ffedae540L,
0xa3f33a8118f9df51L, 0xdd02da5392f8bce5L, 0xf2e637f70a7bd6faL, 0x685c56ca8601c8d7L, 0xc397296903e0b3d8L, 0xe60938944726a567L, 0x4310f514547ef454L,
0x5dfac4e8f9079525L, 0xe7959c6fb2db7225L, 0xd5fd515306230758L, 0x10fc97a7c04e1164L, 0x78070ba095f35bcbL, 0xf6c9c95a2d706830L, 0x94f8aa664c4a986bL,
0x42e81d5f73b14808L, 0x3555355369fe0511L, 0x4cdd8e592dea00beL, 0x9a07d10187e9d922L, 0x7dfc5f23be689befL, 0x6363c19bd833be3aL, 0xfa5fd87b72d19842L,
0xb04ad66c611d175fL, 0x91741ff100b99c1dL, 0xc56e18fd9ab5a36bL, 0xfeedbe55c77d2fa7L, 0x4794d630c5a37ab0L, 0x76e222ebfe86f90eL, 0x5e2c57d0732447d2L,
0x381bb75600ce09d2L, 0x6c9717402ffe3ba2L, 0x18902a4af51c48a9L, 0x269d1b37528b2124L, 0x224f7d41cf40dff7L, 0x2352f6f753dc6642L, 0x5b4ede250fbc1636L,
0x74cb2ba3e8bc13b8L, 0x81568593ba104275L, 0xdde92001dfa00b43L, 0x4f69b8ab685b1572L, 0xc85994815a27f540L, 0x5d14b824e80a8837L, 0xe7464fdeb9175686L,
0xef2b4eb6ed076cc7L, 0x18292902e3d2b995L, 0x80f8e50e65c6b50cL, 0x4377e593f689fcd3L, 0x718308f2257c5a4aL, 0xe7760efe150ce5b7L, 0x40331d2efbad0cb5L,
0x3e470afbb520ed0eL, 0xe506eff66c6c6f71L, 0x4ca4b0f6b1879c40L, 0x107855a52e545ed9L, 0x7fce0d68d94bd43dL, 0xaf0ec27188d7cc2aL, 0x8873914b339b9ccL,
0xb57ee03fd3fe167fL, 0xec95848a10a1e5e8L, 0x6f7b03c77d1160ffL, 0x7118d0873170827L, 0xc884948c1a879296L, 0xb2089f20565b1eeeL, 0xd6bc971ba5339c2aL,
0xa0add6f0169c97ceL, 0xa0a161655b02e8a4L, 0xc2ba1e6c8f6085aaL, 0xed69ee66d34ce425L, 0x593ca3d97ad702eaL, 0x9b2421b199cc73ceL, 0xb551d3a26227db98L,
0x8b07722e75dfd5c9L, 0xaa452dc13abf5764L, 0xf7d43bf792638550L, 0xa4c1c175e95c759dL, 0x239ed738984ad333L, 0xae511a9446fe5bceL, 0xc73e09aba777968cL,
0xd0c90bdcfe44d96eL, 0x4275d7fb13b5f2acL, 0x450009baec26b3c1L, 0xfcfee1e655a45399L, 0x9bbaf97417a95a98L, 0x22d021f4ba7a0cdfL, 0xf4c71cda9576ed3fL,
0xfa50cb4b9a710535L, 0x4c1cce67bfe50996L, 0x228f7927dbf93d28L, 0x8c2d1755c2db918bL, 0x66e2a7e5f69e85aL, 0x8bf3c29d4c0e1378L, 0x3ca180c97ebb2753L,
0xe8281eae32ae6461L, 0xa8904793996680feL, 0x926d802424e90fafL, 0x6ae259d3fef7281fL, 0x35bb82723bfbe8L, 0x33d36bfefc9dcd0bL, 0x7c6af0d34dd15e95L,
0xc424807b4202cdfdL, 0x2b680080aeed83e5L, 0xc99745e2584f8502L, 0x7f404447bf648debL, 0xe78214a4596dbc39L, 0xa09d41483fa401b2L, 0x530b911bbedef6ceL,
0x71bbf802a2fb63f9L, 0x556cd8c8c54de6e1L, 0x18ff267414192349L, 0x2c698665bd940a75L, 0x45954752ac5b5848L, 0x65326ee82e047d93L, 0xd478e0f4d11e941aL,
0x78c9663ba382cc90L, 0x13da002a8183528fL, 0xe98178ef7e119c17L, 0xa065aae5ed3eb488L, 0xf4bfef383e5a6f8eL, 0x94a7bc6f603d9d71L, 0x47b1d61792510eb9L,
0x4cd360b953fc50a0L, 0xaee0d693b01d512bL, 0xf1543905aa2fc955L, 0x8d46c6cec0f4dc48L, 0x194aa9342ab7b23bL, 0x874dd65215ecb504L, 0x6913b3816b2ca9dfL,
0x2bea9eb1511b5e03L, 0x725f2765d2fe43e9L, 0x2ea0f3244a199966L, 0xde758065e5b479d1L, 0xb95ff0cff6dd3dc3L, 0xb4f60408da11725cL, 0xc2a989c5ca2e38c8L,
0xa559a2f7b9514266L, 0x7d6b7e2fcb46076L, 0x68a5778a2301e76fL, 0x37968b096b011d75L, 0x7ad2c0be6374568bL, 0xd3611797da951937L, 0xeda4d5aea91cffe6L,
0x75bc1b48b79f40daL, 0xeb8ec999e5a10de6L, 0x2e5439638c0da31aL, 0x7ed51971282a4c18L, 0x8d4c9f5b29eef7a3L, 0x410db03ff3af3f11L, 0x27f85f6c3a2bac59L,
0xda95e2a866d7ae37L, 0x975aeee7d887f577L, 0x3ddd4a0cb8d9d3daL, 0xd148485023b51eb5L, 0xe70054d5e1b64128L, 0x330b39d04e1b1d03L, 0xef0a822189dc482dL,
0x7f9c3c0173a916deL, 0x50faca6f5be07032L, 0x1c1aad0fe80bc0aaL, 0xe4ba689001b12029L, 0xdbe31a2065c0ce77L, 0x550f3add13358788L, 0x24850728b54a9f68L,
0xe0fda853071afb3aL, 0x33d0530b8d55881cL, 0x472f46330217873dL, 0x9a09d4f4fec9e2e1L, 0x97ae2c0ca3a6823eL, 0x4c41c3a8a5e2d323L, 0xb4099e19e1d35618L,
0xa9cd9f523008eb0dL, 0x422839ca22c4214fL, 0x8dbc13e80839fdaL, 0xbff27499cebd408cL, 0x62ea4226b89a419fL, 0x49d82458d023c44L, 0xb6029268fc9f9baL,
0xa51f2fcab9da2237L, 0x30d72da8a044b884L, 0xdd26b42f5f30548cL, 0x6bc9902a9b0dbe55L, 0xf8d9941bff236f31L, 0xda532cccbaed1eacL, 0x24adee97eae8eab6L,
0xc5bc00ab303e18aaL, 0xe5a0cd0c0ee68505L, 0xb803f83714ba7cd7L, 0xecd22db01daf4be2L, 0xb7e3013063dae0f9L, 0xbeb0d27d9d4f3696L, 0xea90ea8a0cafde39L,
0x55119ec1634a78a6L, 0xe9c876fbdffe6a17L, 0x7260102290559326L, 0x384838973877f11bL, 0xcf0f27ad9ba7d178L, 0x312bff809bf641bbL, 0x6388ed00860e8d6aL,
0x2e49ad1e66103db3L, 0x113a3b7d2ec68c65L, 0x6b9f11e7a1690d6fL, 0x9b86b9531ab34f79L, 0x75405ef8a4e9f72bL, 0xeda01eaab23ac77eL, 0x70720a185fa40fe5L,
0xc677f98b794a3a01L, 0xecc87a715d2173f6L, 0x67a98349635526ebL, 0x4af24668cb1e4c50L, 0x6a4c072e85b31a9bL, 0x7e18d95b990809e9L, 0xb0c1605246e3bf70L,
0x3fc3bf840eee727L, 0xdb95752d76d072eL, 0x85f320ef285deb3eL, 0x88c7dcd0faec9f0aL, 0x8a0b6ccc87650903L, 0xf8ebb1f5c46d8290L, 0xa7810ba50118278cL,
0x976038179d1f9339L, 0x19894d576bfbd737L, 0x501dd63dcb86834bL, 0x93e4cf249a7587f6L, 0x770d569777f4bd9bL, 0xb372e2a560d485d1L, 0x497db9849db462a5L,
0x7a6ee0583af3ca69L, 0x92639f3f6a0f4f9cL, 0x4e5389ea670bc9f8L, 0x5e3fdc068ed5e0c1L, 0x9df0fbe1b66f35d8L, 0xc41abef4990b7e75L, 0x56dc198bbc8234e2L,
0x5480eb6bd18af32dL, 0x5dd9b3bec27bc9caL, 0xc11dfd71a656802cL, 0x3c4074336061bf1fL, 0x88c43cb125091498L, 0xea8cafba3b3d3002L, 0xde44f7a617f86c3dL,
0xe7dd9a9993bab11fL, 0x3a5ea8d4c50e09f5L, 0x4ceaaeaa98ec9705L, 0xe744382cef118f0aL, 0xa8b49a27dae1ced3L, 0x735ff7f4efb65e24L, 0x203f8619fa47a35fL,
0x4556c13218c31160L, 0xc2bbe874fd32a70aL, 0xe4852647a1ca9794L, 0x1202b702d73c17fbL, 0x8e70a33d8c93a08bL, 0xba4edff51d76581bL, 0xfc002332455a1731L,
0x729591de81b7d061L, 0xdf9e9cc050e43e26L, 0x5062a3dfd1c3d976L, 0x8d8813975cccccddL, 0x35e55704f86f6a99L, 0x9fa64d1b1e405b7cL/*, 0x84d2208f5ef0c058L */
};

Logicalboard::Logicalboard()
    : hash(0)
{}

void Logicalboard::apply_move(move_t move)
{
    unsigned char destrank, destfile, sourcerank, sourcefile;
    get_source(move, sourcerank, sourcefile);
    get_dest(move, destrank, destfile);
    piece_t destpiece = get_piece(destrank, destfile);
    piece_t sourcepiece = get_piece(sourcerank, sourcefile);
    piece_t resultpiece = sourcepiece;
    Color color = get_color(sourcepiece);

    set_piece(destrank, destfile, sourcepiece);
    set_piece(sourcerank, sourcefile, EMPTY);

    if (enpassant_file != -1) {
        update_zobrist_hashing_enpassant(enpassant_file, false);
    }

    enpassant_file = -1;

    if ((sourcepiece & PIECE_MASK) == bb_king) {
        set_can_castle(color, true, false);
        set_can_castle(color, false, false);
        // castling
        if (sourcefile == 4 && (destfile == 2 || destfile == 6)) {
            unsigned char rooksourcefile = 8, rookdestfile = 8;
            piece_t castlerook = bb_rook | (color == Black ? BlackMask : 0);

            switch (destfile) {
            case 2:
                rooksourcefile = 0;
                rookdestfile = 3;
                break;
            case 6:
                rooksourcefile = 7;
                rookdestfile = 5;
                break;
            }
            if (rooksourcefile != 8) {
                set_piece(destrank, rookdestfile, castlerook);
                set_piece(sourcerank, rooksourcefile, EMPTY);
                update_zobrist_hashing_piece(destrank, rookdestfile, castlerook, true);
                update_zobrist_hashing_piece(sourcerank, rooksourcefile, castlerook, false);
            }
        }
    }
    else if ((sourcepiece & PIECE_MASK) == bb_rook) {
        switch (sourcefile) {
        case 0:
            set_can_castle(color, false, false);
            break;
        case 7:
            set_can_castle(color, true, false);
            break;
        }
    }
    else if ((sourcepiece & PIECE_MASK) == bb_pawn) {
        // set enpassant capability
        if (abs(destrank - sourcerank) == 2) {
            update_zobrist_hashing_enpassant(destfile, true);
            enpassant_file = destfile;
        }
        // pawn promote
        else if (destrank == 0 || destrank == 7) {
            resultpiece = (get_promotion(move) & PIECE_MASK) | (color == Black ? BlackMask : 0);
            set_piece(destrank, destfile, resultpiece);
        }
        // enpassant capture
        else if (sourcefile != destfile && destpiece == EMPTY) {
            update_zobrist_hashing_piece(sourcerank, destfile, get_piece(sourcerank, destfile), false);
            set_piece(sourcerank, destfile, EMPTY);
        }
    }
    side_to_play = get_opposite_color(color);
    if (color == Black) {
        move_count++;
    }

    // update zobrist hash
    update_zobrist_hashing_piece(sourcerank, sourcefile, sourcepiece, false);
    update_zobrist_hashing_piece(destrank, destfile, destpiece, false);
    update_zobrist_hashing_piece(destrank, destfile, resultpiece, true);
    if ((move & INVALIDATES_CASTLE_K) != 0) {
        update_zobrist_hashing_castle(color, true, true);
    }
    if ((move & INVALIDATES_CASTLE_Q) != 0) {
        update_zobrist_hashing_castle(color, false, true);
    }
    update_zobrist_hashing_move();
    this->in_check = this->king_in_check(this->side_to_play);
    update();

	assert(piece_bitmasks[(bb_king + 1) + bb_king] > 0);
	assert(piece_bitmasks[bb_king] > 0);
    seen_positions.push_back(hash);
}

void Logicalboard::undo_move(move_t move)
{
    unsigned char destrank, destfile, sourcerank, sourcefile;
    get_source(move, sourcerank, sourcefile);
    get_dest(move, destrank, destfile);

    piece_t moved_piece = get_piece(destrank, destfile);
    Color color = get_color(moved_piece);
    piece_t captured_piece = get_captured_piece(move);
    if (captured_piece != EMPTY) {
        captured_piece = make_piece(captured_piece & PIECE_MASK, get_opposite_color(color));
    }
    in_check = move & MOVE_FROM_CHECK;

    // flags
    side_to_play = color;

    if ((move & INVALIDATES_CASTLE_K) != 0) {
        set_can_castle(color, true, true);
        update_zobrist_hashing_castle(color, true, true);
    }
    if ((move & INVALIDATES_CASTLE_Q) != 0) {
        set_can_castle(color, false, true);
        update_zobrist_hashing_castle(color, false, true);
    }

    update_zobrist_hashing_piece(destrank, destfile, moved_piece, false);

    // promote
    piece_t promote = get_promotion(move);
    if (promote & PIECE_MASK) {
        moved_piece = make_piece(bb_pawn, color);
    }

    set_piece(sourcerank, sourcefile, moved_piece);
    set_piece(destrank, destfile, captured_piece);

    // update zobrist hash
    update_zobrist_hashing_piece(sourcerank, sourcefile, moved_piece, true);
    update_zobrist_hashing_piece(destrank, destfile, captured_piece, true);
    update_zobrist_hashing_move();

    // castle: put the rook back
    if ((moved_piece & PIECE_MASK) == bb_king && sourcefile == 4 && (destfile == 2 || destfile == 6)) {
        int rook = make_piece(bb_rook, color);
        unsigned char rook_dest_file = 8, rook_source_file = 8;
        switch (destfile) {
            case 2:
                rook_source_file = 0;
                rook_dest_file = 3;
                break;
            case 6:
                rook_source_file = 7;
                rook_dest_file = 5;
                break;
        }
        set_piece(destrank, rook_source_file, rook);
        set_piece(destrank, rook_dest_file, EMPTY);
        update_zobrist_hashing_piece(destrank, rook_source_file, rook, true);
        update_zobrist_hashing_piece(destrank, rook_dest_file, rook, false);
    }

    if (enpassant_file != -1) {
        update_zobrist_hashing_enpassant(enpassant_file, false);
    }

    // en passant capture
    if (move & ENPASSANT_FLAG) {
        set_piece(sourcerank, destfile, make_piece(bb_pawn, get_opposite_color(color)));
    }
    enpassant_file = (move >> 20) & 0xf;
    if (enpassant_file > 8) {
        enpassant_file = -1;
    } else {
        update_zobrist_hashing_enpassant(enpassant_file, true);
    }
    if (color == Black) {
        move_count--;
    }
    seen_positions.pop_back();
}

void Logicalboard::update_zobrist_hashing_piece(unsigned char rank, unsigned char file, piece_t piece, bool adding)
{
    Color color = get_color(piece);
    int pt = (piece & PIECE_MASK) - 1;
    if (pt >= 0) {
        int posindex = file + rank * 8;
        hash ^= zobrist_hashes[posindex + pt * 64 + color * (64 * 6)];
    }
}

void Logicalboard::update_zobrist_hashing_move()
{
    hash ^= zobrist_hashes[64*6*2];
}

void Logicalboard::update_zobrist_hashing_castle(Color color, bool kingside, bool enabling)
{
    int index = 0;
    if (color == Black) {
        index += 1;
    }
    if (kingside) {
        index += 1;
    }
    hash ^= zobrist_hashes[64*6*2 + 1 + index];
}

void Logicalboard::update_zobrist_hashing_enpassant(int file, bool enabling)
{
    assert(file >= 0 && file < 8);
    hash ^= zobrist_hashes[64*6*2 + 1 + 4 + file];
}
