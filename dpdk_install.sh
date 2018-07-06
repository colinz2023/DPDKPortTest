#!/bin/sh
kPWD=`pwd`
kUpDir=`dirname $kPWD`
kOutPutDir=local
kRTE_SDK_TARGET=x86_64-native-linuxapp-gcc

vFLAG_BUILD_DPDK=0
vFLAG_MLX4=0
vFLAG_MLX5=0
vFLAG_KNI=0
vDPDK_CONFIG=""
vDPDK_VERSON="16.11.7"

declare -A vNameMap=(\
                    ["18.05"]="dpdk-18.05" \
                    ["18.02.2"]="dpdk-stable-18.02.2" \
                    ["17.11.3"]="dpdk-stable-17.11.3" \
                    ["17.05.2"]="dpdk-stable-17.05.2" \
                    ["16.11.7"]="dpdk-stable-16.11.7" \
                    ["16.11.6"]="dpdk-stable-16.11.6" \
                    ["16.11.5"]="dpdk-stable-16.11.5" \
                    ["16.11.4"]="dpdk-stable-16.11.4" \
                    ["16.11.3"]="dpdk-stable-16.11.3" \
                    )

down_dpdk_all()
{
    for i in ${!vNameMap[@]}; do
        down_dpdk $i
    done
}

clear_all_dpdk_files()
{
    for i in ${!vNameMap[@]}; do
        the_dir=$kUpDir/vendors/dpdk-$i
        if [[ -d $the_dir ]]; then
            echo "remove $the_dir ? yes/no : "
            read ans
            if [[ "x"$ans == "xyes" ]]; then
                echo "remove $the_dir"
                rm -rf $kUpDir/vendors/dpdk-$i
            fi
        fi
    done
}


#http://static.dpdk.org/rel/dpdk-16.11.3.tar.xz
down_dpdk()
{
    mkdir -p $kUpDir/vendors
    ver=$1

    if [[ ! -f $kUpDir/vendors/dpdk-$ver.tar.xz ]]; then
        wget http://static.dpdk.org/rel/dpdk-$ver.tar.xz -O  $kUpDir/vendors/dpdk-$ver.tar.xz
    fi

    if [[ ! -d $kUpDir/vendors/dpdk-$ver ]]; then
        tar -xf $kUpDir/vendors/dpdk-$ver.tar.xz -C $kUpDir/vendors
        mv $kUpDir/vendors/${vNameMap[$ver]}  $kUpDir/vendors/dpdk-$ver
    fi
}

mk_dpdk_dir()
{
    dpdk_version=$1
    if [[ ! -d $kUpDir/vendors ]]; then
        echo -e "no vendors dir in $kUpDir !!!"
    fi

    rm -rf ./dpdk

    if [[ ! -d $kUpDir/vendors/dpdk-$dpdk_version ]]; then
        echo "download first, dpdk-$dpdk_version!!!"
        down_dpdk $dpdk_version
    fi

    ln -s $kUpDir/vendors/dpdk-$dpdk_version dpdk 
}

build_dpdk()
{
    build_dir=$kPWD/dpdk
    cd $build_dir
    echo "dpdk dir :$build_dir"
    make -j16 install T=$kRTE_SDK_TARGET DESTDIR=$kOutPutDir $vDPDK_CONFIG
    cd  $kPWD
}

build_dpdk_example()
{
    build_dir=$kPWD/dpdk
    cd $build_dir
    make -j16 T=$kRTE_SDK_TARGET $vDPDK_CONFIG O=$kPWD/examples examples 
    cd  $kPWD
}

parse_args()
{
    for arg in "$@"
    do
        length=${#arg}
        if [[ $length -gt $"3" ]]; then
            str1=${arg:0:4}
            str2=${arg:4}

            if [[ "x$str1" == "xdpdk" ]]; then
                vFLAG_BUILD_DPDK=1
                if [ ! -z "${vNameMap[$str2]}" ]; then
                    vDPDK_VERSON=$str2
                fi
            fi
        fi
       
        if [ $arg == "mlx4" ]; then    
            vFLAG_MLX=1
            vFLAG_BUILD_DPDK=1
            vDPDK_CONFIG+=" CONFIG_RTE_LIBRTE_MLX4_PMD=y "
        fi          

        if [ $arg == "mlx5" ]; then    
            vFLAG_MLX=1
            vFLAG_BUILD_DPDK=1
            vDPDK_CONFIG+=" CONFIG_RTE_LIBRTE_MLX5_PMD=y "
        fi          

        if [ $arg == "kni" ]; then    
            vFLAG_KNI=1
            vFLAG_BUILD_DPDK=1
        fi  

        if [ $arg == "download" ]; then    
            down_dpdk_all;exit 0;
        fi        

        if [ $arg == "example" ]; then    
            build_dpdk_example;exit 0;
        fi

        if [ $arg == "cleanall" ]; then    
            clear_all_dpdk_files;exit 0;
        fi


    done
}

build()
{
    if [[ $vFLAG_KNI==0 ]]; then
        vDPDK_CONFIG+=" CONFIG_RTE_KNI_KMOD=n CONFIG_RTE_LIBRTE_KNI=n "
    fi

    if [[ $vFLAG_BUILD_DPDK == 1 ]]; then
        mk_dpdk_dir $vDPDK_VERSON
        build_dpdk
    fi
}

parse_args $@;
echo "$vDPDK_VERSON"
build
