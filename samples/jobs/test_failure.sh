# Test a failure/alert generation.
#
# The firsttime this job is run it will fail
# because the file it checks doesn't exist. It
# will create the file then fail with exit code 1
# to generate an alert.
#
# The next time the job is run (presumable from an
# alert restart command as that is what we are testing
# the file will be found, so we delete it and return
# exit code 0.
#

testfile='/tmp/failure_test_run_once'

if [ -f ${testfile} ];
then
  rm -f ${testfile}
  exit 0
fi
echo "Test file for job scheduler" > ${testfile}
exit 1
