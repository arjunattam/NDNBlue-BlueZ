#include <stdlib.h>
#include <stdio.h>
#include <ccn/ccn.h>
#include <ccn/uri.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>
#include <fcntl.h>
#include "ndnld.h"

#define D 1
#define RFCOMM_CHANNEL 11

uint8_t service_uuid_int[] = { 0xce, 0x52, 0xcc, 0x20, 0xc2, 0x6e, 0x11, 0xe2, 
           0x8b, 0x8b, 0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66 };

void usage(char* progname) {
  printf("usage\n\t%s ccnx:/test server\n\t%s ccnx:/ndntest client E4:CE:8F:38:C2:6B\n", progname, progname);
}

sdp_session_t *register_service()
{
    uint8_t rfcomm_channel = RFCOMM_CHANNEL;
    const char *service_name = "NDNBlue";
    const char *service_dsc = "Server for NDN over Bluetooth";
    const char *service_prov = "NDN";

    uuid_t root_uuid, l2cap_uuid, rfcomm_uuid, svc_uuid;
    sdp_list_t *l2cap_list = 0, 
               *rfcomm_list = 0,
               *root_list = 0,
               *proto_list = 0, 
               *access_proto_list = 0;
    sdp_data_t *channel = 0, *psm = 0;

    sdp_record_t *record = sdp_record_alloc();

    // set the general service ID
    sdp_uuid128_create( &svc_uuid, &service_uuid_int );
    sdp_set_service_id( record, svc_uuid );

    // make the service record publicly browsable
    sdp_uuid16_create(&root_uuid, PUBLIC_BROWSE_GROUP);
    root_list = sdp_list_append(0, &root_uuid);
    sdp_set_browse_groups( record, root_list );

    // set l2cap information
    sdp_uuid16_create(&l2cap_uuid, L2CAP_UUID);
    l2cap_list = sdp_list_append( 0, &l2cap_uuid );
    proto_list = sdp_list_append( 0, l2cap_list );

    // set rfcomm information
    sdp_uuid16_create(&rfcomm_uuid, RFCOMM_UUID);
    channel = sdp_data_alloc(SDP_UINT8, &rfcomm_channel);
    rfcomm_list = sdp_list_append( 0, &rfcomm_uuid );
    sdp_list_append( rfcomm_list, channel );
    sdp_list_append( proto_list, rfcomm_list );

    // attach protocol information to service record
    access_proto_list = sdp_list_append( 0, proto_list );
    sdp_set_access_protos( record, access_proto_list );

    // set the name, provider, and description
    sdp_set_info_attr(record, service_name, service_prov, service_dsc);
    int err = 0;
    sdp_session_t *session = 0;

    // connect to the local SDP server, register the service record, and 
    // disconnect
    session = sdp_connect( BDADDR_ANY, BDADDR_LOCAL, SDP_RETRY_IF_BUSY );
    err = sdp_record_register(session, record, 0);

    // cleanup
    sdp_data_free( channel );
    sdp_list_free( l2cap_list, 0 );
    sdp_list_free( rfcomm_list, 0 );
    sdp_list_free( root_list, 0 );
    sdp_list_free( access_proto_list, 0 );

    return session;
}

int discover_sdp(char* remote) {
  int channel = 0;

  // returns rfcomm channel
  uuid_t svc_uuid;
  int err;
  bdaddr_t target;
  sdp_list_t *response_list = NULL, *search_list, *attrid_list;
  sdp_session_t *session = 0;
  
  str2ba( remote , &target );
  
  // connect to the SDP server running on the remote machine
  if (D) printf("connecting..\n");
  session = sdp_connect( BDADDR_ANY, &target, SDP_RETRY_IF_BUSY );
  
  // specify the UUID of the application we're searching for
  if (D) printf("uuid\n");
  sdp_uuid128_create( &svc_uuid, &service_uuid_int );
  search_list = sdp_list_append( NULL, &svc_uuid );
  
  // specify that we want a list of all the matching applications' attributes
  uint32_t range = 0x0000ffff;
  attrid_list = sdp_list_append( NULL, &range );
  
  // get a list of service records that have UUID 0xabcd
  err = sdp_service_search_attr_req( session, search_list,	\
				     SDP_ATTR_REQ_RANGE, attrid_list, &response_list);
  sdp_list_t *r = response_list;
  
  // go through each of the service records
  for (; r; r = r->next ) {
    sdp_record_t *rec = (sdp_record_t*) r->data;
    sdp_list_t *proto_list;
    
    // get a list of the protocol sequences
    if( sdp_get_access_protos( rec, &proto_list ) == 0 ) {
      sdp_list_t *p = proto_list;
      
      // go through each protocol sequence
      for( ; p ; p = p->next ) {
	sdp_list_t *pds = (sdp_list_t*)p->data;
	    
	// go through each protocol list of the protocol sequence
	for( ; pds ; pds = pds->next ) {

	  // check the protocol attributes
	  sdp_data_t *d = (sdp_data_t*)pds->data;
	  int proto = 0;
	  for( ; d; d = d->next ) {
	    switch( d->dtd ) { 
	    case SDP_UUID16:
	    case SDP_UUID32:
	    case SDP_UUID128:
	      proto = sdp_uuid_to_proto( &d->val.uuid );
	      break;
	    case SDP_UINT8:
	      if( proto == RFCOMM_UUID ) {
		printf("rfcomm channel: %d\n",d->val.int8);
                channel = (int)d->val.int8;
	      }
	      break;
	    }
	  }
	}
	sdp_list_free( (sdp_list_t*)p->data, 0 );
      }
      sdp_list_free( proto_list, 0 );

    }

    printf("found service record 0x%x\n", rec->handle);
    sdp_record_free( rec );
  }

  sdp_close(session);
  return channel;
}

int main(int argc, char* argv[]) {
  CapsH_drop();
  if (argc < 3) {
    usage(argv[0]);
    return 9;
  }

  int count;
  PollMgr pm = PollMgr_ctor(50);
  CcnCC cc = CcnCC_ctor();
  CcnCC_pollAttach(cc, pm);
  CcnLAC lac = CcnLAC_ctor();
  CcnLAC_initialize(lac, CcnCC_ccndid(cc), pm);

  count = 20;
  while (--count > 0 && !CcnLAC_ready(lac)) {
    PollMgr_poll(pm);
  }
  if (CcnCC_error(cc) || CcnLAC_error(lac) || !CcnLAC_ready(lac)) return 1;

  struct ccn_charbuf* prefix = ccn_charbuf_create();
  ccn_name_from_uri(prefix, argv[1]);
  CcnH_regPrefix(CcnPrefixOp_register, CcnCC_ccnh(cc), CcnCC_ccndid(cc), CcnLAC_faceid(lac), prefix);
  ccn_charbuf_destroy(&prefix);

  Link link;
  if (argc == 3 && strcmp(argv[2], "server") == 0) {
    if (D) printf("Server\n");
    // sdp_session_t* session = register_service();
    // sleep(10);
    // sdp_close(session);
    socklen_t opt = sizeof(struct sockaddr_rc);
    int sock = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
    struct sockaddr_rc loc_addr = { 0 }, rem_addr = { 0 };
    loc_addr.rc_family = AF_BLUETOOTH;
    loc_addr.rc_channel = (uint8_t) RFCOMM_CHANNEL;
    loc_addr.rc_bdaddr = *BDADDR_ANY;
    int res = bind(sock, (struct sockaddr*)&loc_addr, sizeof(struct sockaddr_rc));
    if (res != 0) {
      close(sock);
      printf("Bind error\n");
      return 2;
    }
    if (D) printf("Bind done\n");

    listen(sock, 1);
    if (D) printf("Listening...\n");

    int new_sock = accept(sock, (struct sockaddr*)&rem_addr, &opt);
    if (D) printf("Accepted socket fd: %d\n", new_sock);

    char buf[18] = { 0 };
    ba2str( &rem_addr.rc_bdaddr, buf );
    printf("Remote address: %s\n", buf);

    int sock_flags = fcntl(new_sock, F_GETFL, 0);
    fcntl(new_sock, F_SETFL, sock_flags | O_NONBLOCK);

    NBS nbs = NBS_ctor(new_sock, new_sock, SockType_Stream);
    NBS_pollAttach(nbs, pm);
    link = Link_ctorStream(nbs);
    if (!link) {
      printf("Link error\n");
      return 3;
    }

  } else if (argc == 4 && strcmp(argv[2], "client") == 0) {
    if (D) printf("Client\n");
    int sock = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
    struct sockaddr_rc rem_addr = { 0 };

    str2ba( argv[3], &rem_addr.rc_bdaddr );
    rem_addr.rc_family = AF_BLUETOOTH;
    // rem_addr.rc_channel = discover_sdp( argv[3] );
    rem_addr.rc_channel = 13;

    int res = connect(sock, (struct sockaddr *) &rem_addr, sizeof(struct sockaddr_rc));
    if (res != 0) {
      printf("Connect error\n");
      close(sock);
      return 4;
    }
    printf("Connected\n");

    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    NBS nbs = NBS_ctor(sock, sock, SockType_Stream);
    NBS_pollAttach(nbs, pm);
    link = Link_ctorStream(nbs);
    if (!link) {
      printf("Link error\n");
      return 3;
    }
  }

  NdnlpSvc svc = NdnlpSvc_ctor(lac, link, 0, CMPConn_SentPktsCapacity_default, CMPConn_RetryCount_default, CMPConn_RetransmitTime_default, CMPConn_AcknowledgeTime_default);

  while(true) {
    PollMgr_poll(pm);
    NdnlpSvc_run(svc);
  }

  Link_dtor(link);
  CcnCC_pollDetach(cc, pm);
  CcnCC_dtor(cc);
  PollMgr_dtor(pm);

  return 0;
}
