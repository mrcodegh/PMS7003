<?php
// Receive and save PMS7003 data

require "../weatherstation/wx_keys.php";

$dbname = "pmdata";
$tablename = "OutdoorLog";

//error_log( "rcv_pms7003.php entry.\n" );

if ($_POST['SP_1_0'] < 1000) {
	// Create connection
	$conn = new mysqli($servername, $username, $password, $dbname);
	// Check connection
	if ($conn->connect_error) {
		die("Connect failed: " . $conn->error);
	}

	$sql = "INSERT INTO $tablename " . 
	"(SP_1_0, SP_2_5, SP_10_0, AE_1_0, AE_2_5, AE_10_0, batt, fail, reg_date) VALUES (" .
	$_POST['SP_1_0'].", ".$_POST['SP_2_5'].", ".$_POST['SP_10_0'].", ".$_POST['AE_1_0'].", ".$_POST['AE_2_5'].", ".$_POST['AE_10_0'].", ".
	$_POST['batt'].", ".$_POST['fail'].', "'.gmdate("Y-m-d H:i:s").'")';
	if ($conn->query($sql) === TRUE) {
	//    echo "update<br>";
	    echo "success<br>";
	} else {
		echo "Error: " . $sql . "<br>" . $conn->error;
		error_log( $sql . $conn->error . "\n" );
	}

	$conn->close();
} else {
	echo "failure<br>";
}

?>