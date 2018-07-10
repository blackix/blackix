# Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

use strict;
use warnings;
use Data::Dumper;
use Digest;
use File::Copy;
use File::Path;
use File::Find;
use List::Util ('min', 'max');
use ElectricCommander();
use JSON;
use URI::Escape;
use Cwd;
use HTML::Entities;

# local modules
use Utility;
use Workspace;

# standard colors
my $error_color = '#bd2424';
my $warning_color = '#F9BB00';
my $success_color = '#18a752';
my $unknown_color = '#cccccc';

### Commands ###########################################################################################################################################################################################

# prints the history of builds in the given stream
sub print_build_history
{
	my ($ec, $ec_job, $stream, $node, $count, $before_change) = @_;
	
	my $history = get_build_history($ec, $ec_job, $stream, $node, $count, $before_change, 1);
	foreach(@{$history})
	{
		print "CL $_->{'properties'}->{'CL'}: Job $_->{'job_id'} \"$_->{'job_name'}\", JobStep $_->{'jobstep_id'}, Result = $_->{'result'}\n";
	}
}

# prints a list of all suspected 
sub print_suspected_causers
{
	my ($ec, $jobstep_id, $workspace, $min_change) = @_;
	
	# read the jobstep info from EC
	my $jobstep = ec_get_jobstep($ec, $jobstep_id);

	# read diagnostics for the given jobstep
	my $diagnostics = read_jobstep_diagnostics($jobstep);
	fail("Couldn't read diagnostics file") if !$diagnostics;

	# parse out the files with errors
	my $files = parse_files_containing_errors($workspace->{'dir'}, $diagnostics);
	if(@{$files})
	{
		# print out all the files
		print "\n";
		print "Found file: $_\n" foreach(@{$files});
		print "\n";

		# map them to changelists
		my $change_to_files = find_changes_affecting_files($files, $workspace->{'name'}, $min_change);
		foreach my $change_number(sort { $b <=> $a } keys %{$change_to_files})
		{
			print "Change $change_number:\n";
			foreach my $file (@{$change_to_files->{$change_number}})
			{
				print "    Modifies $file\n";
			}
		}
	}
}

# writes a notification email to the local directory
sub write_step_notification
{
	my ($ec, $ec_project, $jobstep_id, $workspace_name, $workspace_dir, $send_to) = @_;

	# get the metadata for this job step
	my $jobstep = ec_get_jobstep($ec, $jobstep_id);
	
	# read the job definition the original workspace on the network
	my $job_definition_file = join_paths($jobstep->{'workspace_dir'}, 'job.json');
	my $job_definition = (-f $job_definition_file)? read_json($job_definition_file) : undef;
	
	# create some dummy repro steps
	my $repro_steps = "Engine\\Build\\BatchFiles\\RunUAT.bat BuildGraph -Script=Engine/Build/InstalledEngineBuild -Target=".quote_argument($jobstep->{'jobstep_name'})." -P4";

	# format the email
	my $notifications = get_jobstep_notifications($ec, $ec_project, $job_definition, $jobstep, $workspace_name, $workspace_dir, $repro_steps);
	fail("No notifications for jobstep $jobstep_id") if !$notifications;
	print "Default recipients: ".join(", ", @{validate_emails($notifications->{'default_recipients'})})."\n";
	print "Fail Causers: ".join(", ", @{validate_emails($notifications->{'fail_causer_emails'})})."\n";
	
	# write the file to disk
	my $output_file = "StepNotification.html";
	print "Writing $output_file...\n";
	write_file($output_file, $notifications->{'message_body'});

	# send it to the recipients
	if($send_to)
	{
		print "Sending to $send_to...\n";
		my $arguments = {};
		$arguments->{'configName'} = 'EpicMailer';
		$arguments->{'subject'} = "[Build] $jobstep->{'job_name'} (Test)";
		$arguments->{'to'} = $send_to;
		$arguments->{'html'} = $notifications->{'message_body'};
		$ec->sendEmail($arguments);
	}
}

# writes a notification email to the local directory
sub write_report_notification
{
	my ($ec, $ec_project, $job_id, $report_name) = @_;
	
	# get the job details
	my $job = ec_get_job($ec, $job_id);
	
	# read the job definition
	my $job_definition_file = join_paths($job->{'workspace_dir'}, 'job.json');
	my $job_definition = read_json($job_definition_file);
	
	# find the trigger definition
	my $report_definition = find_report_definition($job_definition, $report_name) || fail("Couldn't find definition for $report_name");
	
	# get all the jobsteps for it
	my $report_jobsteps = get_report_jobsteps($job, $report_definition);
	
	# get the notification info
	my $message = get_report_notification($report_definition, $job, $report_jobsteps);
	
	# write the file to disk
	my $output_file = "ReportNotification.html";
	print "Writing $output_file...\n";
	write_file($output_file, $message);
}

### Utility functions ##################################################################################################################################################################################

# format the html header for a notification message
sub get_notification_header
{
	my ($outcome, $title, $summary_lines) = @_;

	# write the header
	my $message = "";
	$message .= "<table width=\"100%\" cellpadding=\"25px\" style=\"background:".(($outcome eq 'success')? $success_color : ($outcome eq 'warning')? $warning_color : $error_color)."; color:#ffffff; min-width:640px;\" border=\"0\">";
	$message .=     "<tr>";
	$message .=         "<td>";
	$message .=             "<table width=\"100%\" cellpadding=\"0px\" cellspacing=\"0px\">";
	$message .=                 "<tr>";
	$message .=                     "<td>";
	$message .=                         "<table cellpadding=\"0px\" cellspacing=\"0px\">";
	$message .=                             "<tr>";
	if($outcome ne 'success')
	{
		$message .= "<td valign=\"middle\"><img src=\"https://cdn2.unrealengine.com/Maintenance/error-414x391-793315300.png\" width=\"42\" height=\"40\" alt=\"Alert\"></td>";
		$message .= "<td width=\"10px\"><font size=\"1px\">&nbsp;</font></td>";
	}
	$message .=                                 "<td><h1 style=\"font-size: 36px; color:white; margin:0px;\">$title</h1></td>";
	$message .=                             "</tr>";
	$message .=	                        "</table>";
	$message .=                     "</td>";
	$message .=                 "</tr>";
	$message .=                 "<tr>";
	$message .=                     "<td style=\"font-size:1px;line-height:25px;\">&nbsp;</td>";
	$message .=                 "</tr>";
	$message .=                 "<tr>";
	$message .=                     "<td>";
	$message .=                         "<table width=\"100%\" cellpadding=\"20px\" style=\"border:2px solid white;\">";
	$message .=                             "<tr>";
	$message .=                                 "<td>";
	$message .=                                     "<table cellpadding=\"2px\">";
	foreach my $summary_line (@{$summary_lines})
	{
		$message .= "<tr>";
		$message .=     "<td style=\"font-size: 11pt;font-weight: bold;text-align: left;vertical-align:top;padding-left:10px;width:150px;color:white;\">$summary_line->[0]</td>";
		$message .=     "<td style=\"font-size: 11pt;vertical-align:top;color:white;\">$summary_line->[1]</td>";
		$message .= "</tr>";
	}
	$message .=                                     "</table>";
	$message .=                                 "</td>";
	$message .=                             "</tr>";
	$message .=                         "</table>";
	$message .=                     "</td>";
	$message .=                 "</tr>";
	$message .=             "</table>";
	$message .=         "</td>";
	$message .=     "</tr>";
	$message .= "</table>";
	
	# prevent gmail optimizing font sizes for mobile
	$message .= "<div style=\"display:none; white-space:nowrap; font:15px courier; line-height:0;\">";
	$message .=     "&nbsp; " x 30;
	$message .= "</div>";
	
	$message;
}

# gets notification information for a report
sub get_report_notification
{
	my ($report_definition, $job, $jobsteps) = @_;

	# get the report name
	my $report_name = $report_definition->{'Name'};
	
	# figure out the overall result
	my $succeeded = ec_get_combined_result($jobsteps);
	
	# create the output message
	my $html = "";
	$html .= "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" \"http://www.w3.org/TR/html4/loose.dtd\">\n";
	$html .= "<html>\n";
	$html .= "<head>\n";
	$html .= "	<title>$report_name</title>\n";
	$html .= "	<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">\n";
	$html .= "</head>\n";
	$html .= "<body style=\"margin:0;font-family: Arial, Helvetica, sans-serif;\">\n";

	# write the header
	my $summary_lines = [];
	push(@{$summary_lines}, [ "Job", "<a href=\"".encode_entities(ec_get_full_url($job->{'job_url'}))."\" style=\"color:white;\">$job->{'job_name'}</a>" ]);
	push(@{$summary_lines}, [ "Outcome", "<b>".($succeeded? ($report_definition->{'IsTrigger'}? "Ready" : "Succeeded") : "Failed")."</b>" ]);
	$html .= get_notification_header($succeeded? 'success' : 'error', $report_name, $summary_lines);

	# find the max length of any dependency name
	$html .= "<table width=\"100%\" cellpadding=\"30px\">";
	$html .=     "<tr>";
	$html .=         "<td>";
	if($report_definition->{'IsTrigger'})
	{
		my $message;
		if($succeeded)
		{
			$message = "The <b>'$report_name'</b> trigger is available for CL $job->{'properties'}->{'CL'} in the $job->{'properties'}->{'Stream'} stream.";
		}
		else
		{
			$message = "The <b>'$report_name'</b> trigger for CL $job->{'properties'}->{'CL'} in the $job->{'properties'}->{'Stream'} stream is blocked due to errors.";
		}
		$html .= "<p style=\"font-size:small;margin:0px;margin-bottom:25px;\">$message <a href=\"".encode_entities(ec_get_full_url($job->{'job_url'}))."\"><b>Show Job</b></a></p>";
	}
	$html .=             "<table align=\"left\" style=\"margin-left:0px;\">";
	foreach my $jobstep (@{$jobsteps})
	{
		my $result = $jobstep->{'result'};
		my ($result_color, $result_text) = ($result eq 'success')? ($success_color, 'Success') : ($result eq 'warning')? ($warning_color, 'Warning') : (($result eq 'skipped')? 'gray' : $error_color, (uc (substr $result, 0, 1)).(substr $result, 1));
		my $link = $jobstep->{'jobstep_url'}? encode_entities(ec_get_full_url($jobstep->{'jobstep_url'})) : undef;
		$html .= "<tr>";
		$html .=     "<td style=\"padding:5px;padding-left:1em;padding-right:3em;background-color:#f0f0f0;font-size:small;\">".($link? "<a href=\"$link\">" : "").$jobstep->{'jobstep_name'}.($link? "</a>" : "")."</td>";
		$html .=     "<td style=\"padding:3px;background-color:$result_color;text-align:center;color:white;min-width:8em;font-size:small;\">".($link? "<a href=\"$link\"" : "<span")." style=\"font-weight:bold;color:white;text-decoration:none;\">$result_text".($link? "</a>" : "</span>")."</td>";
		$html .= "</tr>";
	}
	$html .=             "</table>";
	$html .=         "</td>";
	$html .=     "</tr>";
	$html .= "</table>";
	
	$html .= "</body>\n";
	$html .= "</html>\n";

	# return everything
	$html;
}

# determines the notification settings and generates a notification email for a given build
sub get_jobstep_notifications
{
	my ($ec, $ec_project, $job_definition, $jobstep, $workspace_name, $workspace_dir, $repro_steps) = @_;
	
	# read the step properties
	my $current_change = $jobstep->{'properties'}->{'CL'} || ec_get_property($ec, "/jobs[$jobstep->{'job_id'}]/CL");
	my $stream = $jobstep->{'properties'}->{'Stream'} || ec_get_property($ec, "/jobs[$jobstep->{'job_id'}]/Stream");
	my $is_preflight = !!ec_get_property($ec, "/jobs[$jobstep->{'job_id'}]/Preflight CL");

	# enable autoflush, since we'll be buffered writing to postp
	$|++;
	print "\n";

	# insert a short wait just to make sure the post processor has a chance to flush
	sleep 2;

	# read the postp output from the workspace
	my $diagnostics = read_jobstep_diagnostics($jobstep) || { num_errors => 0, num_warnings => 0, list => [] };

	# find the node, and return if we don't want to notify on warnings
	my $node = find_node_definition($job_definition, $jobstep->{'jobstep_name'});

	# remove all the warnings if we're not interested in them
	my $include_warnings = !$node || $node->{'Notify'}->{'Warnings'};
	if(!$include_warnings)
	{
		$diagnostics->{'num_warnings'} = 0;
		$diagnostics->{'list'} = [ grep { $_->{'type'} ne 'warning' } @{$diagnostics->{'list'}} ];
	}

	# create a digest for the diagnostic messages, and store it on the jobstep. we will use this to reduce duplicate build failures.
	my $digest = 'None';
	if($#{$diagnostics->{'list'}} >= 0)
	{
		my $messages = [];
		foreach my $diagnostic(@{$diagnostics->{'list'}})
		{
			my $message = $diagnostic->{'text'};
			
			$message =~ s/\Q$jobstep->{'job_name'}/<CURRENT JOB NAME>/g;
			$message =~ s/\Q$jobstep->{'jobstep_name'}/<CURRENT JOBSTEP NAME>/g;
			$message =~ s/\Q$jobstep->{'workspace_dir'}/<WORKSPACE DIR>/g;
			$message =~ s/(?<![0-9])$jobstep->{'job_id'}(?![0-9])/<CURRENT JOB ID>/g;
			$message =~ s/(?<![0-9])$jobstep->{'jobstep_id'}(?![0-9])/<CURRENT JOBSTEP ID>/g;
			$message =~ s/(?<![0-9])$current_change(?![0-9])/<CURRENT CHANGE>/g;
			$message =~ s/[0-9]+/<NUM>/g;
			
			push(@{$messages}, $message);
		}

		$messages = [sort(@{$messages})];

		my $digest_builder = Digest->new("SHA-1");
		foreach my $message(@{$messages})
		{
			$digest_builder->add($message);
		}

		$digest = $digest_builder->hexdigest;
		
		my $digest_info = "";
		for(my $message_idx = 0; $message_idx <= $#{$messages}; $message_idx++)
		{
			$digest_info .= "MESSAGE ".($message_idx + 1).":\n";
			for my $message_line(split /\n/, $messages->[$message_idx])
			{
				$digest_info .= "    | $message_line\n";
			}
			$digest_info .= "\n";
		}
		$digest_info .= "DIGEST: $digest\n";
		
		my $digest_file = "$jobstep->{'properties'}->{'diagFile'}.digest";
		print "Writing $digest_file...\n";
		write_file($digest_file, $digest_info);
	}
	ec_set_property($ec, "/jobSteps[$jobstep->{'jobstep_id'}]/Digest", $digest, 1);

	# do not return any notifications unless the build failed
	if($diagnostics->{'num_errors'} == 0 && $diagnostics->{'num_warnings'} == 0)
	{
		return undef;
	}

	# get the build history for this node
	my $build_history = get_build_history($ec, $jobstep->{'job_id'}, $stream, $jobstep->{'jobstep_name'}, 150, $current_change, $include_warnings);

	# do not return any notifications unless the previous build failed
#	if($diagnostics->{'num_errors'} == 0 && $diagnostics->{'num_warnings'} == 0)
#	{
#		my $last_build_success = 1;
#		foreach my $build (@{$build_history})
#		{
#			$last_build_success = 0 if lc $build->{'result'} eq 'warning';
#			$last_build_success = 0 if lc $build->{'result'} eq 'error';
#			last if $last_build_success == 0 || lc $build->{'result'} eq 'success';
#		}
#		return undef if $last_build_success;
#	}

	# standard colors
	my $outcome_color = ($diagnostics->{'num_errors'} > 0)? $error_color : ($diagnostics->{'num_warnings'} > 0)? $warning_color : $success_color;
	my $outcome_color_lookup = { error => $error_color, warning => $warning_color, success => $success_color };
	
	# get the outcome description
	my $outcome = "Succeeded";
	if($diagnostics->{'num_errors'} > 0)
	{
		$outcome = "<span style=\"font-weight:bold;\">Failed</span> ($diagnostics->{'num_errors'}".(($diagnostics->{'num_errors'} == 50)? "+" : "")." Errors";
		$outcome .= ", $diagnostics->{'num_warnings'}".(($diagnostics->{'num_warnings'} == 50)? "+" : "")." warnings" if $diagnostics->{'num_warnings'} > 0;
		$outcome .= ")";
	}
	elsif($diagnostics->{'num_warnings'} > 0)
	{
		$outcome = "Completed with $diagnostics->{'num_warnings'}".(($diagnostics->{'num_warnings'} == 50)? "+" : "")." warnings";
	}

	# find the last change that build successfully
	my $last_successful_change = $current_change;
	if($#{$build_history} >= 0)
	{
		$last_successful_change = $build_history->[$#{$build_history}]->{'properties'}->{'CL'};
	}
	
	# if we're including headers in the output message, figure out which paths we want to filter
	my $author_paths = [ ".../Source/...", ".../Build/..." ];
	my $default_recipients = [];
	if($node)
	{
		my $notify = $node->{'Notify'};
		if($notify)
		{
			$default_recipients = split_list($notify->{'Default'} || '', ";");
			my $author_paths_string = $notify->{'Submitters'};
			$author_paths = split_list($author_paths_string, ";") if defined $author_paths_string;
		}
	}

	# get the p4 history from this change
	my $change_history = get_change_history($stream, $author_paths || [ "..." ], $last_successful_change + 1, 1000);
	
	# exclude all the changes before the change we're currently building; it's confusing.
	$change_history = [grep { $_->{'number'} <= $current_change } @{$change_history}];
	
	# find all the fail causers
	mark_potential_fail_causers($change_history, $build_history);

	# add all the unique authors from the change history to the list of recipients
	my $submitters = [];
	my $muted_submitters = [];
	my $fail_causer_users = [];
	my $fail_causer_emails = [];
	my $muted_by_message = "";
	if($author_paths)
	{
		# create a dummy property to make sure the muted sheet exists
		my $muted_path = "/projects[$ec_project]/Generated/".escape_stream_name($stream)."/Muted";
		$ec->setProperty({ propertyName => "$muted_path/_placeholder", value => '' });

		# find the muted list of recipients for this node
		my $muted_sheet = ec_get_property_sheet($ec, { path => $muted_path }) || {};

		# get the current node name
		my $node_name = $node? $node->{'Name'} : $jobstep->{'jobstep_name'};

		# list of people that have claimed ownership
		my $author_claims = {};

		# create a list of failure causers
		my $author_to_notify = {};
		foreach my $change(@{$change_history})
		{
			my $author = $change->{'author'};
			if(!$author_to_notify->{$author} && $change->{'possible_fail_causer_author'})
			{
				my $is_muted = 0;
				eval
				{
					my $muted_user_json = $muted_sheet->{lc $change->{'author_email'}};
					if($muted_user_json)
					{
						my $muted_user = decode_json($muted_user_json);
						if($muted_user->{'version'} == 2)
						{
							my $muted_change = $muted_user->{'muted'}->{$node_name};
							my $claimed_change = $muted_user->{'claimed'}->{$node_name};
							my $default_muted_change = $muted_user->{'muted'}->{'__default__'};
							my $default_claimed_change = $muted_user->{'claimed'}->{'__default__'};
							if($muted_change && $muted_change >= $change->{'number'})
							{
								$is_muted = 1;
							}
							elsif($claimed_change && $claimed_change >= $last_successful_change)
							{
								$author_claims->{$author} = 1;
							}
							elsif($default_muted_change && $default_muted_change >= $change->{'number'})
							{
								$is_muted = 1;
							}
							elsif($default_claimed_change && $default_claimed_change >= $last_successful_change)
							{
								$author_claims->{$author} = 1;
							}
						}
						
					}
				};
				$author_to_notify->{$author} = !$is_muted;
			}
		}

		# build a list of failure causers
		if(%{$author_claims})
		{
			$fail_causer_users = [keys %{$author_claims}];
		}
		else
		{
			$fail_causer_users = [grep { $author_to_notify->{$_} } keys %{$author_to_notify}];
		}

		# build a list of failure causer emails
		my $author_to_email = {};
		foreach my $change(@{$change_history})
		{
			my $author = $change->{'author'};
			my $author_email = $change->{'author_email'};
			$author_to_email->{$author} = $author_email;
		}
		$fail_causer_emails = [map { $author_to_email->{$_} } @{$fail_causer_users}];

		# build a list of all the submitters
		$submitters = [map { $author_to_email->{$_} } keys %{$author_to_notify}];

		# build a list of all non-muted users
		my $muted_submitter_names = [grep { !$author_to_notify->{$_} } keys %{$author_to_notify}];

		# build a list of all non-muted users
		$muted_submitters = [map { $author_to_email->{$_} } @{$muted_submitter_names}];

		# Print the list of muted users for this Build
		if(%{$author_claims})
		{
			print "\n**** Users who have claimed ownership ***************************\n\n";
			print "       $_\n" foreach(keys %{$author_claims});
			$muted_by_message = "Claimed by ".join(", ", map { "<a href=\"mailto:$author_to_email->{$_}\" style=\"color:white;\">$_</a>" } keys %{$author_claims})." - ";
		}
		elsif(@{$muted_submitter_names})
		{
			print "\n**** Users with muted notifications ***************************\n\n";
			print "       $_\n" foreach(@{$muted_submitter_names});
			$muted_by_message = "Muted by ".join(", ", map { "<a href=\"mailto:$author_to_email->{$_}\" style=\"color:white;\">$_</a>" } @{$muted_submitter_names})." - ";
		}
	}

	# print the list of builds
	if($#{$build_history} >= 0)
	{
		# print the list of changes interleaved with the results of each build
		print "\n**** Changes since last succeeded *****************************\n\n";
		print_interleaved_history($build_history, $change_history, { current_jobstep => $jobstep->{'jobstep_id'} });
		print "\n***************************************************************\n\n";
	}

	# find which changes look like they caused failures, based on the files they touched
	my $suspected_files = parse_files_containing_errors($workspace_dir, $diagnostics);
	my $suspected_changes = find_changes_affecting_files($suspected_files, $workspace_name, $last_successful_change + 1);
	
	# write the job info
	my $title = $jobstep->{'jobstep_name'};

	# opening boilerplate
	my $html = "";
	$html .= "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" \"http://www.w3.org/TR/html4/loose.dtd\">\n";
	$html .= "<html>\n";
	$html .= "<head>\n";
	$html .= "	<title>$title</title>\n";
	$html .= "	<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">\n";
	$html .= "</head>\n";
	$html .= "<body style=\"margin:0;font-family: Arial, Helvetica, sans-serif;\">\n";

	# write the heading
	my $summary = [];
	push(@{$summary}, [ "Job", "<a href=\"".encode_entities(ec_get_full_url($jobstep->{'job_url'}))."\" style=\"color:white;\">$jobstep->{'job_name'}</a>" ]);
	push(@{$summary}, [ "Step", "<a href=\"".encode_entities(ec_get_full_url($jobstep->{'jobstep_url'}))."\" style=\"color:white;\">$jobstep->{'jobstep_name'}</a> (<a href=\"".encode_entities(ec_get_full_url($jobstep->{'jobstep_log_url'}))."\" style=\"color:white;\">Output</a>)" ]);
	push(@{$summary}, [ "Outcome", $outcome ]);
	if($node)
	{
		my $notifications_list = $muted_by_message;
		$notifications_list .= "<a href=\"".ec_get_full_url("/commander/pages/NotificationSettings-1.0/NotificationSettings_run?stream=$stream&mute-step=".encode_form_parameter($jobstep->{'jobstep_name'}))."\" style=\"color:white;\">Mute</a>";
		$notifications_list .= " | <a href=\"".ec_get_full_url("/commander/pages/NotificationSettings-1.0/NotificationSettings_run?stream=$stream&mute-branch=1")."\" style=\"color:white;\">Mute Entire Branch</a>";
		$notifications_list .= " | <a href=\"".ec_get_full_url("/commander/pages/NotificationSettings-1.0/NotificationSettings_run?stream=$stream")."\" style=\"color:white;\">Settings...</a>";
		push(@{$summary}, [ "Notifications", $notifications_list ]);
	}

	$html .= get_notification_header(($diagnostics->{'num_errors'} > 0)? 'error' : ($diagnostics->{'num_warnings'} > 0)? 'warning' : 'success', $title, $summary);

	# css styles. gmail strips out all <style> tags, so we have to have to inline them.
	my $section_style = "overflow:auto; border-spacing:0; border-collapse:collapse; table-layout:fixed; min-height:200px; padding-bottom:30px; padding-left:30px; padding-right:30px;";
	
	# write the error report
	my $diagnostics_list = $diagnostics->{'list'};
	if($#{$diagnostics_list} >= 0)
	{
	$html .= "<table cellpadding=\"20px\" width=\"100%\" style=\"min-width:640px;\">";
	$html .=     "<tr>";
	$html .=         "<td>";
	$html .=             "<table cellpadding=\"0px\" width=\"100%\">";
	$html .=                 "<tr>";
	$html .=                     "<td><h2 style=\"color: #202020;margin:0px;font-size: 16pt;\">Report</h2></td>";
	$html .=                 "</tr>";
	$html .=                 "<tr>";
	$html .=                     "<td>";
	$html .=                         "<table cellspacing=\"6px\">";
	$html .=                             "<tr>";
	$html .=                                 "<td>";
	$html .=                                      "<table cellspacing=\"4px\">";

	if($jobstep->{'properties'}->{'ReportBody'})
	{
		$html .=                                          "<tr>";
		$html .=                                              "<td style=\"font-size:11pt; display:table-cell; padding:0px; background:#d6d6d6;\"><div style=\"width:8px;height:1px;\"></div></td>";
		$html .=                                              "<td style=\"font-size:11pt; vertical-align:middle; padding:.65em 1em;\">";
		$html .=                                                  "<p style=\"font-size:small; color:#000000; margin-top:6px; margin-bottom:3px;\">";
		$html .=                                                      $jobstep->{'properties'}->{'ReportBody'};
		$html .=                                                  "</p>";
		$html .=                                              "</td>";
		$html .=                                          "</tr>";
	}
	
	# write the error report
	for(my $diagnostic_idx = 0; $diagnostic_idx <= $#{$diagnostics_list}; )
	{
		my @escaped_text;
			
		# try to merge all the identical diagnostics on consecutive lines together
		my $first_diagnostic = $diagnostics_list->[$diagnostic_idx];
		my $diagnostic_line = $first_diagnostic->{'first_line'};
		while($diagnostic_idx <= $#{$diagnostics_list})
		{
			# get the next diagnostic, and make sure it's the same type
			my $diagnostic = $diagnostics_list->[$diagnostic_idx];
			last if $diagnostic->{'first_line'} != $diagnostic_line || $diagnostic->{'type'} ne $first_diagnostic->{'type'};

			# append all the output lines to the list
			foreach(split /\n/, $diagnostic->{'text'})
			{
				# previous code used to insert a zero-width space at every slash, so we can word wrap long file names, but
				# this can cause really hard to debug errors if copy/pasting the path due to invisible characters
				my $escaped_line = encode_entities($_);
				# $escaped_line =~ s/([\\\/])/&#8203;$1/g;
				push(@escaped_text, $escaped_line);
				$diagnostic_line++;
			}
				
			# move to the next one
			$diagnostic_idx++;
				
			# always keep separate for now
			last;
		}
		
		# remove any blank lines at the end of the output
		while($#escaped_text >= 0 && $escaped_text[$#escaped_text] =~ /^\s*$/)
		{
			pop(@escaped_text);
		}

		# remove any whitespace prefix common to all lines in the output
		my $whitespace_len;
		for(@escaped_text)
		{
			if(!/^\s*$/)
			{
				/^(\s*)/;
				my $line_whitespace_len = length($1);
				$whitespace_len = $line_whitespace_len if !defined $whitespace_len || $line_whitespace_len < $whitespace_len;
			}
		}
		@escaped_text = map { ($whitespace_len < length($_))? (substr $_, $whitespace_len) : '' } @escaped_text if defined $whitespace_len;

		# write out these lines
		my $line_url = "$jobstep->{'jobstep_log_url'}&firstLine=$first_diagnostic->{'first_line'}&numLines=$first_diagnostic->{'num_lines'}";
		$html .= "<tr>";
		$html .=     "<td style=\"font-size:11pt; display:table-cell; padding:0px; background:".($outcome_color_lookup->{$first_diagnostic->{'type'}} || $error_color).";\"><div style=\"width:8px;height:1px;\"></div></td>";
		$html .=     "<td style=\"font-size:11pt; vertical-align:middle; padding:.65em 1em;\">";
		$html .=         "<p style=\"font-size:x-small; color:#000000; margin-bottom:3px;\"><a href=\"".encode_entities(ec_get_full_url($line_url))."\" style=\"color:black;\">[Line $first_diagnostic->{'first_line'}]</a></p>";
		foreach(@escaped_text)
		{
			/( *)(.*)/;
			my $indent = length($1);
			my $line = $2 || "&nbsp;";
			$html .= "<pre style=\"font-family:monospace,Arial,Helvetica,sans-serif;font-size:9pt;margin-top:0px;margin-bottom:1px;padding-left:${indent}ch;white-space:pre-wrap;tab-size:4;\">$line</pre>";
		}
		$html .=     "</td>";
		$html .= "</tr>";
	}

	$html .=                                     "</table>";
	$html .=                                 "</td>";
	$html .=                             "</tr>";
	$html .=                         "</table>";
	$html .=                     "</td>";
	$html .=                 "</tr>";
	$html .=             "</table>";
	$html .=         "</td>";
	$html .=     "</tr>";
	$html .= "</table>";
	}

	# write the list of changes
	my $timeline_change_style = "color:#ffffff; text-decoration:none;";
	my $timeline_item_border_style = "border:2px solid #f7f7f7;";
	my $timeline_point_style = "width:20px; height:20px; border:4px solid #ccc; border-radius:999px;";
	my $timeline_identifier_style = "background:#ccc; padding:5px; width:70px; text-align:center; margin-left:10px; margin-right:10px; font-size:12px; $timeline_item_border_style";
	$html .= "<table cellpadding=\"20px\" width=\"100%\" bgcolor=\"#f7f7f7\" style=\"min-width:640px;\">";
	$html .=     "<tr>";
	$html .=         "<td>";
	$html .=             "<table cellpadding=\"0px\">";
	$html .=                 "<tr>";
	$html .=                     "<td><h2 style=\"color: #202020;font-size: 16pt; margin:0px;\">Timeline</h2></td>";
	$html .=                 "</tr>";
	$html .=                 "<tr>";
	$html .=                     "<td>";
	$html .=                         "<table cellspacing=\"0\" cellpadding=\"0\" style=\"border-collapse:collapse; margin-top:15px; margin-left:10px; margin-right:10px;\">";

	my $spacer_html = '';
	$spacer_html .= "<tr>";
	$spacer_html .=     "<td>&nbsp; </td>";
	$spacer_html .=     "<td width=\"12\"></td>";
	$spacer_html .=     "<td width=\"4\" style=\"background:#ccc;\"><div style=\"width:4px; height:20px;\">&nbsp;</div></td>";
	$spacer_html .=     "<td width=\"12\"></td>";
	$spacer_html .=     "<td>&nbsp; </td>";
	$spacer_html .=     "<td>&nbsp; </td>";
	$spacer_html .= "</tr>";
	
	my $build_idx = 0;
	my $change_idx = 0;
	my $num_omitted_changes = 0;
	while($build_idx <= $#{$build_history} || $change_idx <= $#{$change_history})
	{
		my $build = ($build_idx <= $#{$build_history})? $build_history->[$build_idx] : undef;
		my $change = ($change_idx <= $#{$change_history})? $change_history->[$change_idx] : undef;
		
		if($change && $build && !$change->{'possible_fail_causer'} && $build->{'properties'}->{'CL'} < $change->{'number'})
		{
			$num_omitted_changes++;
			$change_idx++;
			next;
		}

		if($num_omitted_changes > 0)
		{
			$html .= $spacer_html;

			$html .= "<tr>";
			$html .=     "<td>&nbsp; </td>";
			$html .=     "<td width=\"12\"></td>";
			$html .=     "<td width=\"4\" style=\"background:#ccc;\"><div style=\"width:4px; height:20px;\">&nbsp;</div></td>";
			$html .=     "<td width=\"12\"></td>";
			$html .=     "<td style=\"vertical-align:top;\" colspan=\"3\">";
			$html .=         "<div style=\"$timeline_item_border_style background:#eee;color: black;text-decoration: none;padding:5px 15px 5px 15px;margin-left:10px; margin-right:10px;font-size:12px;display:inline-block;\">$num_omitted_changes ".(($num_omitted_changes == 1)? 'change' : 'changes')." producing identical output</div>";
			$html .=     "</td>";
			$html .= "</tr>";
			
			$num_omitted_changes = 0;
		}

		$html .= $spacer_html if $build_idx > 0 || $change_idx > 0;
		
		if(!$change || ($build && $build->{'properties'}->{'CL'} >= $change->{'number'}))
		{
			my $build_color = $outcome_color_lookup->{$build->{'result'}} || (($build->{'jobstep_id'} == $jobstep->{'jobstep_id'})? $outcome_color : $unknown_color);
			$html .= "<tr>";
			$html .=     "<td style=\"vertical-align:top;\">";
			$html .=         "<div style=\"$timeline_identifier_style margin-left:0px; background:$build_color;\"><a href=\"".encode_entities(ec_get_full_url($build->{'jobstep_url'}))."\" style=\"color: white;text-decoration: none;\">".(($build->{'jobstep_id'} == $jobstep->{'jobstep_id'} && $is_preflight)? "Preflight" : "$build->{'properties'}->{'CL'}")."</a></div>";
			$html .=     "</td>";
			$html .=     "<td colspan=\"3\">";
			$html .=         "<div style=\"$timeline_point_style background:$build_color; border-color:$build_color;\"></div>";
			$html .=     "</td>";
			$html .= "</tr>";
			$build_idx++;
		}
		else
		{
			my $escaped_description = $change->{'description'};
			$escaped_description =~ s/\n.*//g;
			$escaped_description = substr($escaped_description, 0, 250);
			$escaped_description = encode_entities($escaped_description);

			my $suspected_change_note = '';
			if($suspected_changes->{$change->{'number'}})
			{
				my @file_names = map { /([^\\\/]*)$/ && $1 } @{$suspected_changes->{$change->{'number'}}};
				$suspected_change_note = "<div style=\"background: #FFF075;padding: 5px;padding-left: 10px;padding-right: 10px;display:inline-block;$timeline_item_border_style\">Modifies ".join(", ", @file_names)."</div>";
			}
			
			# add the changelist number
			$html .= "<tr>";
			$html .=     "<td>&nbsp; </td>";
			$html .=     "<td colspan=\"3\" height=\"20px\">";
			$html .=         "<div style=\"$timeline_point_style\"></div>";
			$html .=     "</td>";
			$html .=     "<td style=\"vertical-align:top;\">";
			$html .=         "<div style=\"$timeline_identifier_style\"><a href=\"https://p4-swarm.epicgames.net/changes/$change->{'number'}\" style=\"color: black;text-decoration: none;\">$change->{'number'}</a></div>";
			$html .=     "</td>";
			$html .=     "<td rowspan=\"2\" style=\"vertical-align:top;\">";
			$html .=         "<div style=\"color: black;font-size: 12px;\"><div style=\"padding:6px;display:inline-block;\"><a href=\"mailto:$change->{'author_email'}\">$change->{'author'}</a> - $escaped_description</div>$suspected_change_note</div>";
			$html .=     "</td>";
			$html .= "</tr>";

			$change_idx++;
		}
	}
	$html .=                         "</table>";
	$html .=                     "</td>";
	$html .=                 "</tr>";
	$html .=             "</table>";
	$html .=         "</td>";
	$html .=     "</tr>";
	$html .= "</table>";
		
	# write the steps to reproduce
	if($repro_steps && $#{$diagnostics_list} >= 0)
	{
		$html .= "<table cellpadding=\"20px\" width=\"100%\" bgcolor=\"#fcfcfc\" style=\"min-width:640px;\">";
		$html .=     "<tr>";
		$html .=         "<td>";
		$html .=             "<table cellpadding=\"0px\" width=\"100%\">";
		$html .=                 "<tr>";
		$html .=                     "<td><h2 style=\"color: #202020;font-size: 16pt; margin:0px;\">Steps to Reproduce</h2></td>";
		$html .=                 "</tr>";
		$html .=                 "<tr>";
		$html .=                     "<td>";
		$html .=                         "<table cellspacing=\"6px\">";
		$html .=                             "<tr>";
		$html .=                                 "<td>";
		$html .=                                     "<table cellspacing=\"4px\">";
		$html .=                                         "<tr>";
		$html .=                                             "<td style=\"font-size:11pt; display:table-cell; padding:0px; background:$outcome_color;\"><div style=\"width:8px;height:1px;\"></div></td>";
		$html .=                                             "<td style=\"font-size:11pt; vertical-align:middle; padding:.65em 1em;\">";
		$html .=                                                 "<p style=\"font-family:Arial,Helvetica,sans-serif;font-size:10pt;margin-top:0px;margin-bottom:10px;white-space:pre-wrap;tab-size:4;\">To execute this step locally, run the following from a ".(is_windows()? "command prompt" : "terminal session").":</p>";
		$html .=                                                 "<p style=\"font-family:monospace,Arial,Helvetica,sans-serif;font-size:9pt;margin-top:0px;margin-bottom:1px;white-space:pre-wrap;tab-size:4;\">$repro_steps</p>";
		$html .=                                             "</td>";
		$html .=                                         "</tr>";
		$html .=                                     "</table>";
		$html .=                                 "</td>";
		$html .=                             "</tr>";
		$html .=                         "</table>";
		$html .=                     "</td>";
		$html .=                 "</tr>";
		$html .=             "</table>";
		$html .=         "</td>";
		$html .=     "</tr>";
		$html .= "</table>";
	}
		
	# closing boilerplate
	$html .= "</body>\n";
	$html .= "</html>\n";
	
	$html =~ s/    /\t/gm;
	
	# return all the settings
	{ default_recipients => $default_recipients, submitters => $submitters, muted_submitters => $muted_submitters, last_successful_change => $last_successful_change, fail_causer_users => $fail_causer_users, fail_causer_emails => $fail_causer_emails, message_body => $html, outcome => ($diagnostics->{'num_errors'}? 'error' : $diagnostics->{'num_warnings'}? 'warning' : 'success') };
}

# gets the history of builds for a given node, stopping at the last successful build before the current one
sub get_build_history
{
	my ($ec, $ec_job, $stream, $node, $count, $before_change, $include_warnings) = @_;

	# find a recent list of job steps with the right name
	my $jobsteps_xpath = $ec->findObjects("jobStep", { maxIds => "$count", numObjects => "$count", 
		filter => [{propertyName => 'stepName', operator => 'equals', operand1 => $node}, {propertyName => 'Stream', operator => 'equals', operand1 => $stream} ],
		  sort => [{propertyName => 'jobId', order => 'descending'}],
		select => [{propertyName => 'CL'}, {propertyName => 'Digest'}] });

	# manually join the jobsteps with the list of jobs
	my $history = [];
	foreach my $object_node ($jobsteps_xpath->findnodes("/responses/response/object"))
	{
		my $jobstep_node = $jobsteps_xpath->findnodes("./jobStep", $object_node)->get_node(1);
		my $jobstep = ec_parse_jobstep($jobsteps_xpath, $jobstep_node);
		next if $jobstep->{'job_name'} =~ /Preflight/i && $jobstep->{'job_id'} != $ec_job;
		$jobstep->{'properties'} = ec_parse_property_sheet($jobsteps_xpath, $object_node);
		push(@{$history}, $jobstep);
		last if !$include_warnings && $jobstep->{'result'} eq 'warning';
		last if $jobstep->{'result'} eq 'success' && (!$before_change || $jobstep->{'properties'}->{'CL'} < $before_change);
	}
		
	# return the results sorted in decending changelist order
	$history = [ sort { $b->{'properties'}->{'CL'} <=> $a->{'properties'}->{'CL'} || $b->{'job_id'} <=> $a->{'job_id'} } @{$history} ];
}

# gets the changes in a given stream, along with the emails of each submitter
sub get_change_history
{
	my ($stream, $filter_list, $first_change_number, $max_results) = @_;

	# get the range of changes to look for
	my $change_range = $first_change_number? "\@$first_change_number,now" : "";
	
	# query perforce for the list of changes, running through each filter at a time
	my @change_history = ();
	my %unique_change_numbers = ();
	foreach my $filter(@{$filter_list})
	{
		my $current_change;
		foreach(p4_command("changes -l -m $max_results $stream/$filter$change_range"))
		{
			if(/^Change (\d+) on [^ ]+ by ([^ ]+)@/)
			{
				$current_change = { number => $1, author => $2, description => '' };
				if($2 ne 'buildmachine' && !$unique_change_numbers{$1})
				{
					push(@change_history, $current_change);
					$unique_change_numbers{$1} = 1;
				}
			}
			elsif($current_change && /^\t(.*)/)
			{
				$current_change->{'description'} .= "$1\n" if $1 || $current_change->{'description'};
			}
		}
	}
	
	# replace any ROBOMERGE changes with the original author
	foreach my $change(@change_history)
	{
		my $description = $change->{'description'};
		if($description =~ /^#ROBOMERGE-OWNER:\s+([^\s]+)\s*(.*)$/m || $description =~ /^#ROBOMERGE-AUTHOR:\s+([^\s]+)\s*(.*)$/m)
		{
			$change->{'author'} = $1;
			$description =~ s/^#ROBOMERGE-(AUTHOR|OWNER).*$//m;
			$description =~ s/^\s*//;

			my ($robomerge_change) = $description =~ /^#ROBOMERGE-SOURCE: CL (\d+)/m;
			$change->{'description'} = "ROBOMERGE".($robomerge_change? " CL $robomerge_change" : "").": $description";
		}
	}

	# find the email addresses for each user
	my $p4_user_to_info = {};
	foreach my $change(@change_history)
	{
		my $user = $change->{'author'};
		
		my $info = $p4_user_to_info->{$user};
		if(!$info)
		{
			my @records = p4_tagged_command("user -o \"$user\"");

			$info->{'user'} = $records[0]->{'Type'} ? $records[0]->{'User'} : $user;
			$info->{'email'} = $records[0]->{'Type'} ? $records[0]->{'Email'} : 'Build@epicgames.com';
			$info->{'full_name'} = $records[0]->{'Type'} ? $records[0]->{'Full Name'} : $user;

			$p4_user_to_info->{$user} = $info;
		}
		
		$change->{'author'} = $info->{'user'};
		$change->{'author_email'} = $info->{'email'};
		$change->{'author_full_name'} = $info->{'full_name'};
	}

	# return the changes sorted in descending changelist order
	@change_history = sort { $b->{'number'} <=> $a->{'number'} } @change_history;
	\@change_history;
}

# mark all the changes that are within a range that are possible fail causers (determined by those which cause notifications to change)
sub mark_potential_fail_causers
{
	my ($change_history, $build_history) = @_;

	# mark all the changes that potentially caused the failure, due to the digest changing
	my $last_notifications_digest = '';
	my $change_idx = $#{$change_history};
	for(my $build_idx = $#{$build_history}; $build_idx >= 0; $build_idx--)
	{
		my $next_notifications_digest = $build_history->[$build_idx]->{'properties'}->{'Digest'};
		if(defined $next_notifications_digest)
		{
			my $possible_fail_causer = $next_notifications_digest ne $last_notifications_digest;
			for(; $change_idx >= 0 && $change_history->[$change_idx]->{'number'} <= $build_history->[$build_idx]->{'properties'}->{'CL'}; $change_idx--)
			{
				$change_history->[$change_idx]->{'possible_fail_causer'} = $possible_fail_causer;
			}
			$last_notifications_digest = $next_notifications_digest;
		}
	}
	
	# mark all the changes by users that are possible fail causers
	my $fail_causer_authors = {};
	foreach my $change(@{$change_history})
	{
		$fail_causer_authors->{lc $change->{'author'}} = 1 if $change->{'possible_fail_causer'};
	}
	foreach my $change(@{$change_history})
	{
		$change->{'possible_fail_causer_author'} = defined $fail_causer_authors->{lc $change->{'author'}};
	}
}

# find files containing errors in the given diagnostic output
sub parse_files_containing_errors
{
	my ($workspace_dir, $diagnostics) = @_;

	my $base_dir = join_paths($workspace_dir, 'Engine', 'Source');
	
	my $suspected_files = { };
	foreach my $diagnostic (@{$diagnostics->{'list'}})
	{
		foreach(split /\n/, $diagnostic->{'text'})
		{
			if(/([^ \\\/]*[\\\/][^ ()]*)\([\d,]+\)\s*:\s*(error|warning)[\s:]/)
			{
				# Visual studio style error; "<FileName>(<Line>): error"
				my $full_path = File::Spec->rel2abs($1, $base_dir);
				$suspected_files->{$full_path} = 1;
			}
			elsif(/^\s*(\/[^: ][^:]+):[\d:]+:\s+(error|warning):/)
			{
				# Clang style error; "<FileName>:<Line/Column>: error"
				my $full_path = File::Spec->rel2abs($1, $base_dir);
				$suspected_files->{$full_path} = 1;
			}
			elsif(/^[A-Za-z_]+:(?:Warning|Error): ((?:[A-Za-z]:\\|\/)[^:]+):/) 
			{
				# Engine asset error; <LogName>:Error: <FileName>:"
				my $full_path = File::Spec->rel2abs($1, $base_dir);
				$suspected_files->{$full_path} = 1;
			}
		}
	}

	[keys %{$suspected_files}];
}

# find changes that any of the given files
sub find_changes_affecting_files
{
	my ($files, $workspace_name, $min_change) = @_;

	# convert the suspected files into a list of potential changelists
	my $change_to_files = { };
	foreach my $file (@{$files})
	{
		foreach(p4_command("-c$workspace_name filelog -m 10 \"$file\"", {errors_as_warnings => 1}))
		{
			if(/^#\d+ change (\d+) /)
			{
				last if $1 < $min_change;
				push(@{$change_to_files->{$1}}, $file);
			}
		}
	}
	$change_to_files;
}

# prints the build history and change history to the log together
sub print_interleaved_history
{
	my ($build_history, $change_history, $optional_arguments) = @_;
	
	my $change_idx = 0;
	foreach my $build (@{$build_history})
	{
		# print all the changes after this build
		while($change_idx <= $#{$change_history} && $change_history->[$change_idx]->{'number'} > $build->{'properties'}->{'CL'})
		{
			my $change = $change_history->[$change_idx++];
			
			my $short_description = $change->{'description'};
			$short_description =~ s/[\r\n]+/ /g;
			$short_description = substr($short_description, 0, 250);
			
			my $fail_causer = '  ';
			if($change->{'possible_fail_causer'})
			{
				$fail_causer = '>>';
			}

			print "            $fail_causer $change->{'number'} $change->{'author_email'} $short_description$fail_causer\n";
		}

		# get a description for the current build
		my $message;
		if($optional_arguments->{'current_jobstep'} && $build->{'jobstep_id'} == $optional_arguments->{'current_jobstep'})
		{
			$message = "  >>>> $build->{'properties'}->{'CL'} THIS BUILD";
		}
		else
		{
			$message = "       $build->{'properties'}->{'CL'} ".(uc $build->{'result'})." ($build->{'jobstep_id'})";
		}

		# append the notifications suffix
		my $digest = $build->{'properties'}->{'Digest'};
		if($digest)
		{
			$message .= " [$digest]";
		}

		# print it out
		print "$message\n";
	}
}

# read diagnostic output from the given xpath
sub read_jobstep_diagnostics
{
	my ($jobstep) = @_;

	# find the name of the diagnostics output file
	my $diagnostics_file = $jobstep->{'properties'}->{'diagFile'};

	# parse the output
	my $diagnostics;
	if($diagnostics_file)
	{
		# read the file from the workspace
		my $full_diagnostics_file = join_paths($jobstep->{'workspace_dir'}, $diagnostics_file);
		my $diagnostics_text = read_file($full_diagnostics_file);
		my $diagnostics_xpath = XML::XPath->new(xml => $diagnostics_text);

		# set the initial state
		$diagnostics = { num_errors => 0, num_warnings => 0, list => [] };
		
		# loop through all the output
		foreach my $diagnostics_node($diagnostics_xpath->findnodes("/diagnostics/diagnostic"))
		{
			my $type = $diagnostics_xpath->findvalue("./type", $diagnostics_node)->string_value();
			my $first_line = $diagnostics_xpath->findvalue("./firstLine", $diagnostics_node)->string_value();
			my $num_lines = $diagnostics_xpath->findvalue("./numLines", $diagnostics_node)->string_value();
			my $text = $diagnostics_xpath->findvalue("./message", $diagnostics_node)->string_value();
			push(@{$diagnostics->{'list'}}, { type => $type, first_line => $first_line, num_lines => $num_lines, text => $text });
			$diagnostics->{'num_errors'}++ if $type eq 'error';
			$diagnostics->{'num_warnings'}++ if $type eq 'warning';
		}
	}
	$diagnostics;
}

1;
