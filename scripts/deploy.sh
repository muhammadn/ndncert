#!/usr/bin/env bash

echo "What is the CA Prefix (eg. /example) you want to deploy?"
read CA_PREFIX
echo ""

echo "Do you want to compile and build NDNCert?"
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

# Beginning
echo "==================================================================="
echo "=="
echo "== Deploying NDNCERT"
echo "=="
echo "==================================================================="
echo ""
echo "Are you sure [Y/n] ?"
read DEPLOY

case $DEPLOY in
             N|n)
                   echo "Deployment cancelled"
                   exit
             ;;
             Y|y)
             ;;
             *)
                   echo "Unknown option, deployment cancelled"
                   exit
             ;;
esac

echo ""
echo "==================================================================="
echo "=="
echo "== Deployment started"
echo "=="
echo "==================================================================="

if id ndn &>/dev/null; then
    echo 'ndn user account found, GOOD!'
    echo ""
else
    echo 'ndn user not found; adding ndn user as root'
    echo ""
    sudo useradd ndn
fi

echo "Listing all NDNSEC identities"
echo ""
sudo -u ndn ndnsec-ls-identity -k

echo "Do you want to create NDNSEC Keys and Certificate or ${CA_PREFIX}? (Y/n)"
echo ""
read GEN_KEY

case $GEN_KEY in
             N|n)
                   echo "We will not be generating new NDNSEC Key for ${CA_PREFIX}"
             ;;
             Y|y)
                   echo "Generating NDNSEC Key for ${CA_PREFIX}"
                   #sudo -u ndn ndnsec key-gen -tr $CA_PREFIX > default.ndncertreq
		   #sudo -u ndn ndnsec cert-gen -s $CA_PREFIX - < default.ndncertreq > default.ndncert
                   mkdir -p /var/lib/ndn/ndncert-ca
                   sudo chown ndn /var/lib/ndn/ndncert-ca
                   sudo HOME=/var/lib/ndn/ndncert-ca -u ndn ndnsec-keygen $CA_PREFIX
             ;;
             *)
                   echo "Unknown option, deployment cancelled"
                   exit
             ;;
esac

# get the identity name (eg. /example, extract only "example")
IDENTITY=$(echo $CA_PREFIX | sed -e 's/^\///')
#sudo cp /usr/local/etc/ndncert/ca.conf.sample /usr/local/etc/ndncert/ca.$(echo ${IDENTITY}).conf
#CA_CONFIG_PATH=/usr/local/etc/ndncert/ca.$(echo ${IDENTITY}).conf
CA_CONFIG_PATH=/usr/local/etc/ndncert/ca.conf

sed '/"ca-prefix":/ s/"ca-prefix":[^,]*/"ca-prefix":'"\"$IDENTITY\""'/' $CA_CONFIG_PATH

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

# TODO (need client?)
echo "Do you want to configure NDNCert Client?"
echo ""
read $NDNCERT_CLIENT_INSTALL

case $NDNCERT_CLIENT_INSTALL in
             N|n)
                   echo "We will not install systemd on this machine"
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

echo "==================================================================="
echo "=="
echo "== Deployment finished"
echo "=="
echo "==================================================================="
