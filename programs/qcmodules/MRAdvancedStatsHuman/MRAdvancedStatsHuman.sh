# for ado2:
#FSLDIR=/usr/local/fsl; PATH=${FSLDIR}/bin:${PATH}; . ${FSLDIR}/etc/fslconf/fsl.sh; export FSLDIR PATH;

# for cluster:
PATH=/opt/afni:/opt/fbirn_qa/bin:${PATH};
export PATH;

perl MRAdvancedStatsHuman.pl $1