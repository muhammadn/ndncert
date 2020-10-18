#!/usr/bin/env bash

function ndncert_compile() {
  echo "Do you want to compile and build NDNCert? [Y/n]"
  read NDNCERT_COMPILE
  echo ""

  case $NDNCERT_COMPILE in
             N|n)
                   echo "Okay, we'll continue with the setup"
             ;;
             Y|y)
                   cd ../ && CXXFLAGS="-O2" ./waf configure --prefix=/usr --sysconfdir=/etc
                   ./waf
		   echo "Need sudo to install NDNCert CLI tools"
		   sudo ./waf install
             ;;
             *)
                   echo "Unknown option, build and install is cancelled"
                   exit
             ;;
  esac
}

echo "==================================================================="
echo "=="
echo "== Deployment started"
echo "=="
echo "==================================================================="

check_ndn_user() {
  if id ndn &>/dev/null; then
      echo 'ndn user account found, GOOD!'
      echo ""
  else
      echo 'ndn user not found; adding ndn user'
      echo ""
      sudo useradd ndn
  fi

  echo ""
}

function install_ndncert_ca_and_client() {
  echo "What is the CA Prefix (eg. /example) you want to deploy?"
  read CA_PREFIX
  echo ""

  echo "What is the path of your ca.conf file?"
  read CA_CONFIG_PATH
  echo ""

  echo "Do you want to create NDNCert Keys and Certificate or ${CA_PREFIX}? (Y/n)"
  echo ""
  read GEN_KEY

  case $GEN_KEY in
             N|n)
                   echo "We will not be generating new NDNSEC Key for ${CA_PREFIX}"
             ;;
             Y|y)
                   echo "Generating NDNSEC Certificates and Keys for ${CA_PREFIX}"
                   if [[ "$OSTYPE" == "darwin"* ]]; then
		     # Mac uses keychain (and brew installed)
                     ndnsec key-gen -tr $CA_PREFIX > default.ndncertreq
	             ndnsec cert-gen -s $CA_PREFIX - < default.ndncertreq > default.ndncert
                     echo "Listing all NDNSEC identities"
		     echo ""
                     ndnsec-ls-identity -k
                   else
		     # assume Linux
                     sudo -u ndn ndnsec key-gen -tr $CA_PREFIX > default.ndncertreq
                     sudo -u ndn ndnsec cert-gen -s $CA_PREFIX - < default.ndncertreq > default.ndncert
                     echo "Listing all NDNSEC identities"
                     echo ""
                     sudo -u ndn ndnsec-ls-identity -k
		   fi

		   ## This follows the documentation of ndncert
                   #mkdir -p /var/lib/ndn/ndncert-ca
                   #sudo chown ndn /var/lib/ndn/ndncert-ca
                   #sudo HOME=/var/lib/ndn/ndncert-ca -u ndn ndnsec-keygen $CA_PREFIX

                   # get the identity name (eg. /example, extract only "example")
                   #IDENTITY=$(echo $CA_PREFIX | sed -e 's/^\///')

                   ## commented codes if you want a more dynamic configuration (like separate configurations)
                   #sudo cp /usr/local/etc/ndncert/ca.conf.sample /usr/local/etc/ndncert/ca.$(echo ${IDENTITY}).conf
                   #CA_CONFIG_PATH=/usr/local/etc/ndncert/ca.$(echo ${IDENTITY}).conf
		   if [ -f "$CA_CONFIG_PATH" ]; then
                     CONFIG=$(jq -r --arg CA_PREFIX "$CA_PREFIX" '."ca-prefix" = $CA_PREFIX' $CA_CONFIG_PATH)
                     echo $CONFIG > $CA_CONFIG_PATH
		   else
		     echo ""
                     echo "File ${CA_CONFIG_PATH} does not exist!" 
		     echo ""
                   fi

		   # For systemd, only applicable for Linux
		   # For other systems please start with "ndncert-ca-server -c /usr/local/etc/ndncert/ca.conf"
		   if [[ "$OSTYPE" == "linux-gnu"* ]]; then
                      service ndncert-ca start
		   fi

		   # NDNCert Client config (Since we already have the cert installed in CA)
                   echo "Do you want to configure NDNCert Client?"
                   echo ""
                   read NDNCERT_CLIENT_INSTALL

                   case $NDNCERT_CLIENT_INSTALL in
                                               N|n)
                                                      echo "OK, we will not configure NDNCert for client"
                                               ;;
                                               Y|y)
                                                      echo "Configuring NDNCert for Client"
                                                      echo "Where is the path of your NDN client.conf?"
                                                      read NDNCLIENT_CONFIG_PATH
                                                      CERTIFICATE=$(cat default.ndncert | { tr -d '\n'; })
						      NDNCERT_CONFIG=$(jq -r --arg CERTIFICATE "$CERTIFICATE" --arg CA_PREFIX "$CA_PREFIX" '."ca-list" += [{"ca-prefix":$CA_PREFIX, "ca-info":"NDNCERT CA for \($CA_PREFIX)", "max-validity-period": "129600", "max-suffix-length":"2", "probe-parameters": [{"probe-parameter-key":"email"}],"certificate":$CERTIFICATE}]' $NDNCLIENT_CONFIG_PATH)
						      #NDNCERT_CONFIG=$(jq -r --arg CERTIFICATE "$CERTIFICATE" CA_PREFIX "$CA_PREFIX" '."ca-list"[] | select(."ca-prefix" == $CA_PREFIX) | .certificate = $CERTIFICATE' $NDNCLIENT_CONFIG_PATH)
             			                      # NDNCERT_CONFIG=$(jq -r --arg CERTIFICATE "$CERTIFICATE" '."ca-list"[0].certificate = $CERTIFICATE' $CA_CONFIG_PATH)
                                                      echo $NDNCERT_CONFIG > $NDNCLIENT_CONFIG_PATH
                        
						      echo "Let's start configuring NDNCert client for ${CA_PREFIX}"

						      ndncert-client
                                                ;;
                                                *)
                                                      echo "Unknown option, deployment cancelled"
                                                      exit
                                                ;;
                   esac

             ;;
             *)
                   echo "Unknown option, deployment cancelled"
                   exit
             ;;
  esac
}

function install_systemd() {
  echo "Do you want to install ndncert CA for systemd on this machine?"
  echo ""
  read SYSTEMD_INSTALL

  case $SYSTEMD_INSTALL in
             N|n)
                   echo "We will not install systemd CA on this machine"
             ;;
             Y|y)
                   echo "Copying NDNCERT-CA systemd service on this machine"
		   sudo cp ../systemd/ndncert-ca.service /etc/systemd/system
		   sudo chmod 644 /etc/systemd/system/ndncert.service
             ;;
             *)
                   echo "Unknown option, deployment cancelled"
                   exit
             ;;
  esac
}

# Steps on script
ndncert_compile
check_ndn_user
install_ndncert_ca_and_client
# only for Linux, not applicable to other operating systems
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
  install_systemd
fi

echo "==================================================================="
echo "=="
echo "== Deployment finished"
echo "=="
echo "==================================================================="
