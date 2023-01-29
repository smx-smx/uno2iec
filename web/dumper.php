<?php
/**
 * Copyright(C) 2023 Stefano Moioli <smxdev4@gmail.com>
 */

abstract class Constants {
	/**
	 * Path to the 'disccopy' command
	 */
	const ExeFile = '/home/pi/uno2iec2/commandline/build/disccopy';
	/**
	 * Directory where to write resulting d64 images
	 */
	const OutDir = '/var/www/html/c64_images';
	const LogFile = '/tmp/disccopy.log';
	const PidFile = '/tmp/dumper.pid';
	/**
	 * GPIO pin tied to the Arduino reset line (active low)
	 */
	const ArduinoResetGpio = '23';
}

/**
 * Sanitizes the incoming user-supplied filename
 */
function sanitize_filename(string $file){
	$file = mb_ereg_replace("([^\w\s\d\-_~,;\[\]\(\).])", '', $file);
	$file = mb_ereg_replace("([\.]{2,})", '', $file);
	return $file;
}

function path_combine(string ...$parts){
	return implode(DIRECTORY_SEPARATOR, $parts);
}

/**
 * Obtains the image filename from user parameters
 */
function get_filename(){
	$fname = sanitize_filename(basename($_GET['filename']));
	return pathinfo($fname, PATHINFO_FILENAME) . '.d64';
}

/**
 * Reads the pid file
 */
function read_status(&$status){
	$data = file_get_contents(Constants::PidFile);
	if($data === false) return false;
	$status = explode("\0", $data, 2);
	return true;
}

/**
 * Checks if disccopy is running
 * @param string $filename (if running): the current filename being dumped
 * @param int $pid (if running): the current dumper PID
 * @return bool whether there is an active dumper or not
 */
function is_running(&$filename, &$pid){
	$pidFile = Constants::PidFile;
	$running = false;
	if(file_exists($pidFile)){
		if(read_status($status)){
			list($pid, $filename) = $status;
			$running = posix_kill($pid, 0);
			if(!$running){
				unlink($pidFile);
			}
		}
	}
	return $running;
}

/**
 * Executes the supplied command (and arguments)
 * The output is logged to the log file
 */
function cmd(string ...$args){
	$proc = proc_open($args, [
		'1' => ['file', Constants::LogFile, 'a'],
		'2' => ['file', Constants::LogFile, 'a'],
	], $pipes);
	proc_close($proc);
}

/**
 * Logs a message to the log file
 */
function logmsg(string $msg){
	file_put_contents(Constants::LogFile, $msg . "\n", FILE_APPEND);
}

/**
 * Resets the Arduino MCU
 */
function arduino_reset(){
	$gpio = Constants::ArduinoResetGpio;
	cmd('gpioset', '0', "{$gpio}=0");
	sleep(1);
	cmd('gpioset', '0', "{$gpio}=1");
}

/**
 * Handler for the 'start' action
 */
function start(){
	$running = is_running($filename, $pid);
	$outcome = [
		'running' => $running
	];
	if($running) goto output;

	logmsg('Resetting Arduino');
	arduino_reset();
	
	$filename = get_filename();
	$fullPath = path_combine(Constants::OutDir, $filename);

	$cmdline = implode(' ', array_map('escapeshellarg', [
		Constants::ExeFile,
		'--serial', '/dev/ttyAMA0',
		'--speed', '57600',
		'--source', '8',
		'--target', $fullPath,
	]));

	$proc = proc_open([
		'/bin/sh', '-c', (''
			. 'exec ' . $cmdline
			. ' >>' . escapeshellarg(Constants::LogFile)
			. ' 2>&1 & echo $!'
		)
	], [
		'1' => ['pipe', 'w']
	], $pipes);
	
	$pid = rtrim(fgets($pipes[1]));
	proc_close($proc);

	file_put_contents(Constants::PidFile,
		$pid . "\0" . $fullPath
	);

	$outcome['pid'] = $pid;
	$outcome['filename'] = $fullPath;

	
	for($i=0; $i<5; $i++){
		if(file_exists($filename)) break;
		sleep(1);
	}
	$outcome['running'] = file_exists($fullPath);
	

output:
	header('Content-Type: application/json');
	print(json_encode($outcome));
	ob_flush();

	if($proc !== false){
		proc_close($proc);
	}
}


/**
 * Handler for the 'status' action
 */
function status(){
	$running = is_running($filename, $pid);

	$size = file_exists($filename)
		? filesize($filename)
		: 0;
	
	$json = json_encode([
		'running' => $running,
		'filename' => $filename,
		'size' => $size
	]);
	header('Content-Type: application/json');
	print($json);
}

/**
 * Handler for the 'stop' action
 */
function stop(){
	if(is_running($filename, $pid)){
		cmd('kill', '-9', $pid);
		is_running($filename, $pid);
	}
}

/**
 * Handler for the 'poweroff' action
 */
function poweroff(){
	cmd('sudo', 'poweroff');
}

switch($_GET['action']){
	case 'start': return start();
	case 'status': return status();
	case 'stop': return stop();
	case 'poweroff': return poweroff();
	case 'reset': return arduino_reset();
	default: return http_response_code(400);
}