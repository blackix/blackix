# Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

use strict;
use warnings;
use Data::Dumper;
use File::Copy;
use File::Path;
use File::Find;
use List::Util ('min', 'max');
use ElectricCommander();
use JSON;
use URI::Escape;

require Exporter;
our @EXPORT = (
		qw(setup_autosdk_environment setup_autosdk_workspace sync_autosdk_workspace),
		qw(setup_temporary_workspace setup_workspace clean_workspace sync_workspace),
		qw(conform_resources conform_resource)
	);

# common sync arguments. previously enabled parallel syncing, disabled for now due to apparent race condition
# creating directories on Mac.
my $sync_arguments = ""; #	"--parallel=threads=8,min=1,minsize=1";
	
##### AutoSDKs #####################################################################################################################################################################################

# setup the autosdk environment variable
sub setup_autosdk_environment
{
	my ($root_dir) = @_;

	my $autosdk_dir = join_paths($root_dir, 'AutoSDK');
	print "Using SDKs in $autosdk_dir\n";
	$ENV{'UE_SDKS_ROOT'} = $autosdk_dir;
}

# create a client and sync the platform sdks
sub setup_autosdk_workspace
{
	my ($root_dir) = @_;
	
	# get the p4 environment settings
	my %p4_info = %{(p4_tagged_command('info -s'))[0]};
	my $p4_user_name = $p4_info{'userName'};
	my $p4_host_name = $p4_info{'clientHost'};

	# remove any domain from the host name (which happens on Mac)
	my $workspace_host_name = $p4_host_name;
	$workspace_host_name =~ s/\..*$//;
	$workspace_host_name = uc $workspace_host_name;
	
	# get all the workspace settings)
	my $workspace_name = "Builder+$workspace_host_name+AutoSDK";
	my $workspace_dir = join_paths($root_dir, 'AutoSDK');
	
	my $workspace_name_regex = quotemeta $workspace_name;

	# if the workspace directory doesn't already exist (eg. builder has been formatted), delete the existing 
	# client to be sure that it's re-synced from scratch.
	if(!-d $workspace_dir)
	{
		# if this is a brand new machine we won't have a workspace to delete and p4 will error out, so make sure it exists.
		my @existing_clients = p4_command("clients -e $workspace_name");
		if(grep (m/^Client $workspace_name_regex /, @existing_clients))
		{
			p4_command("client -d $workspace_name");
		}
	}

	# create or update the client
	my $start_time = time;
	print "Updating $workspace_name...\n";

	# get the default view for the autosdk workspace
	my $template_name = is_windows()? 'Builder-AutoSDK-Windows' : 'Builder-AutoSDK-Mac';
	my @template_spec_lines = p4_command("client -o $template_name");
	my $template_spec = p4_parse_spec(@template_spec_lines);

	# create a new clientspec file
	my @new_spec = ();
	push(@new_spec, "Client: $workspace_name\n");
	push(@new_spec, "Owner: $p4_user_name\n");
	push(@new_spec, "Root: $workspace_dir\n");
	push(@new_spec, "Host: $p4_host_name\n");
	push(@new_spec, "View:\n");
	foreach my $template_view_line(split /\n/, $template_spec->{'View'})
	{
		$template_view_line =~ s/\/\/$template_name\//\/\/$workspace_name\//;
		push(@new_spec, "\t$template_view_line\n");
	}
	push(@new_spec, "Options: rmdir clobber\n");

	# create or update the client
	print "Updating $workspace_name...\n";
	p4_command_with_input('client -i', @new_spec);

	# make sure it succeeded
	my @clients = p4_command("clients -e $workspace_name");
	fail("Failed to create client $workspace_name") if !grep (m/^Client $workspace_name_regex /, @clients);
	print "Completed in ".(time - $start_time)."s.\n";

	# setup the environment 
	setup_autosdk_environment($root_dir);
	
	# return the new workspace info
	{ name => $workspace_name, dir => $workspace_dir };
}

# sync the latest platform sdks
sub sync_autosdk_workspace
{
	my ($workspace) = @_;

	# sync the latest from this branch
	my $start_time = time;
	print "Syncing $workspace->{'name'} to latest.\n";
	p4_command("-c$workspace->{'name'} sync -q $sync_arguments //...");
	print "Completed in ".(time - $start_time)."s.\n\n";
}

##### Stream Workspaces ############################################################################################################################################################################

# creates a temporary workspace used for querying stream metadata. should not be used for syncing.
sub setup_temporary_workspace
{
	my ($stream) = @_;
	
	# get the p4 environment settings
	my %p4_info = %{(p4_tagged_command('info -s'))[0]};
	my $p4_user_name = $p4_info{'userName'};
	my $p4_host_name = $p4_info{'clientHost'};

	# create or update a workspace to reference the current stream. we'll never sync it, but we need it to query changes.
	my $workspace_name = "Builder+$p4_host_name+Temp";

	# create a workspace for this stream
	my @new_spec = ();
	push(@new_spec, "Client: $workspace_name\n");
	push(@new_spec, "Owner: $p4_user_name\n");
	push(@new_spec, "Root: .\n");
	push(@new_spec, "Host: $p4_host_name\n");
	push(@new_spec, "Stream: $stream\n");
	p4_command_with_input('client -i', @new_spec);

	# return the new workspace name
	$workspace_name;
}

# setup a workspace for the current stream
sub setup_workspace
{
	my ($root_dir, $stream, $optional_arguments) = @_;

	my $start_time = time;

	# get the p4 environment settings
	my %p4_info = %{(p4_tagged_command('info -s'))[0]};
	my $p4_user_name = $p4_info{'userName'};
	my $p4_host_name = $p4_info{'clientHost'};

	# use the stream name without any leading slashes as the default workspace identifier
	my $workspace_identifier = $stream;
	$workspace_identifier =~ s/^\/\///;

	# get the stream to start with
	my $workspace_stream = $stream;
	
	# whether the workspace should sync incrementally
	my $workspace_incremental_sync = 0;
	
	# the local DDC path to use for this workspace
	my $workspace_local_ddc;
	
	# read the agent settings, if necessary
	my $workspace_exclusions;
	my $workspace_additional_view;
	if($optional_arguments->{'agent_type'})
	{
		my ($ec, $ec_project, $agent_type) = ($optional_arguments->{'ec'}, $optional_arguments->{'ec_project'}, $optional_arguments->{'agent_type'});
	
		# read the settings for this branch
		my $settings = read_stream_settings($ec, $ec_project, $stream);
		
		# treat the agent type 'Initial' as a special reference to the initial agent type
		$agent_type = $settings->{'Initial Agent Type'} if lc $agent_type eq 'initial';

		# make sure the agent type exists
		fail("No agent type '$agent_type' found for $stream") if !$settings->{'Agent Types'}->{$agent_type};

		# read the settings for a local ddc, if present
		$workspace_local_ddc = $settings->{'Agent Types'}->{$agent_type}->{'LocalDDC'};
		
		# parse the json and get the definition for this agent type
		my $workspace_name = $settings->{'Agent Types'}->{$agent_type}->{'Workspace'};
		if(defined $workspace_name)
		{
			# append it to the workspace name
			$workspace_identifier .= "+$workspace_name";

			# get the workspace settings
			my $workspace_settings = $settings->{'Workspaces'};
			fail("Missing workspace settings") if !defined $workspace_settings;
			$workspace_settings = $workspace_settings->{$workspace_name};
			fail("Missing workspace settings for $workspace_name") if !defined $workspace_settings;

			# allow overriding the workspace name to facilitate switching between streams
			if($workspace_settings->{'Identifier'})
			{
				$workspace_identifier = $workspace_settings->{'Identifier'};
			}
			
			# allow overriding the stream with another stream, which may narrow the view
			if($workspace_settings->{'Stream'})
			{
				$workspace_stream = $workspace_settings->{'Stream'};
			}

			# check whether it prefers incremental syncs
			if($workspace_settings->{'Incremental Sync'})
			{
				$workspace_incremental_sync = 1;
			}

			# read the exclusions
			$workspace_exclusions = $workspace_settings->{'Exclude'};

			# read the additional mappings
			$workspace_additional_view = $workspace_settings->{'View'};
		}
	}

	# replace invalid characters in the workspace identifier with a '+' character
	$workspace_identifier =~ s/[\/\\:<>]/+/g;
	
	# append the slot index, if it's non-zero
	my $slot_idx = $optional_arguments->{'slot_idx'} || 0;
	$workspace_identifier .= sprintf("+%02d", $slot_idx) if $slot_idx;
	
	# remove any domain from the host name (which happens on Mac)
	my $workspace_host_name = $p4_host_name;
	$workspace_host_name =~ s/\..*$//;
	$workspace_host_name = uc $workspace_host_name;
	
	# get all the workspace settings
	my $workspace_name = "Builder+$workspace_host_name+$workspace_identifier";
	my $workspace_metadata_dir = join_paths($root_dir, "++$workspace_identifier");
	my $workspace_dir = join_paths($workspace_metadata_dir, 'Sync');
	my $workspace_change_filename = join_paths($workspace_metadata_dir, 'Workspace.txt');
	my $workspace_capture_filename = join_paths($workspace_metadata_dir, 'Workspace.dat');
	print "Updating $workspace_name...\n";

	# create an explicit view for this workspace, if defined by the exclude settings
	my @workspace_view;
	if($workspace_exclusions || $workspace_additional_view)
	{
		# get the default stream view
		my @stream_spec_output = p4_command("stream -o -v $workspace_stream");
		my $stream_spec = p4_parse_spec(@stream_spec_output);
		my @stream_view = split /\s*\n\s*/, $stream_spec->{'View'};

		# get the depot prefix from the stream name
		my ($depot_prefix) = $stream =~ /(\/\/[^\/]+)/;
		fail("Couldn't parse depot name from '$stream'") if !defined $depot_prefix;

		# add all the stream view lines to the workspace view
		foreach my $stream_line(@stream_view)
		{
			my @clauses = split(/\s+/, $stream_line);
			fail("Cannot parse line in stream view: '$stream_line'; found $#clauses clauses.") if $#clauses != 1;
			push(@workspace_view, "$clauses[0] //$workspace_name/$clauses[1]");
		}

		# add all the standard exclusions
		if($workspace_exclusions)
		{
			foreach my $workspace_exclusion(@{$workspace_exclusions})
			{
				$workspace_exclusion =~ s/^\s+//;
				$workspace_exclusion =~ s/\s+$//;
				$workspace_exclusion =~ s/^(\*|\.\.\.)//;
				push(@workspace_view, "-$depot_prefix/...$workspace_exclusion //$workspace_name/...$workspace_exclusion") if length($workspace_exclusion) > 0;
			}
		}

		# add any additional mappings
		if($workspace_additional_view)
		{
			foreach(@{$workspace_additional_view})
			{
				next if /^\s*$/;

				# separate the view into three parts; an optional '-' character, a source, and optional target.
				my ($exclude_prefix, $source, $target) = /^\s*(-?)\s*([^\s]+)\s*(?:([^\s]*)?\s*)$/;

				# make sure everything begins with a slash
				$source = "/$source" if $source !~ /^\//;
				$target = "/$target" if $target && $target !~ /^\//;

				# convert it into an absolute depot path
				$source = "$stream$source" if $source !~ /^\/\//;

				# set the target path
				if(!$target)
				{
					$target = $source;
					$target =~ s/^\/\/[^\/]+\/[^\/]+//;
				}

				# add the final line
				push(@workspace_view, "$exclude_prefix$source //$workspace_name$target");
			}
		}
	}
	
	# check if the client already exists
	my @clients = p4_command("clients -e $workspace_name");
	my $workspace_name_regex = quotemeta $workspace_name;
	if(grep(m/^Client $workspace_name_regex /, @clients))
	{
		# delete everything in the workspace
		p4_revert_all_changes($workspace_name);

		# read the current clientspec
		my @current_spec_lines = p4_command("client -o $workspace_name");
		my $current_spec = p4_parse_spec(@current_spec_lines);

		# if the owner, host or root doesn't match, delete the workspace and start again
		if($current_spec->{'Owner'} ne $p4_user_name || $current_spec->{'Root'} ne $workspace_dir || $current_spec->{'Host'} ne $p4_host_name)
		{
			# delete the existing client
			p4_command("client -d $workspace_name");

			# delete the directory
			safe_delete_directory($workspace_dir) if -d $workspace_dir;
		}
	}
	else
	{
		# delete the directory
		safe_delete_directory($workspace_dir) if -d $workspace_dir;
	}

	# create a new clientspec file
	my @new_spec = ();
	push(@new_spec, "Client: $workspace_name\n");
	push(@new_spec, "Owner: $p4_user_name\n");
	push(@new_spec, "Root: $workspace_dir\n");
	push(@new_spec, "Host: $p4_host_name\n");
	if($#workspace_view < 0)
	{
		push(@new_spec, "Stream: $workspace_stream\n");
	}
	else
	{
		push(@new_spec, "View:\n");
		foreach my $workspace_view_line (@workspace_view)
		{
			push(@new_spec, "\t$workspace_view_line\n");
		}
	}
	push(@new_spec, "Options: rmdir clobber\n");
	
	# create or update the client
	p4_command_with_input('client -i', @new_spec);

	# create a p4.ini file, so we can easily execute p4 commands when fixing problems
	my $p4_ini_filename = join_paths($workspace_metadata_dir, 'p4.ini');
	mkdir $workspace_metadata_dir;
	open(my $p4_ini_file_handle, '>', $p4_ini_filename) or fail("Failed to open $p4_ini_filename.");
	print $p4_ini_file_handle "P4USER=$p4_user_name\n";
	print $p4_ini_file_handle "P4CLIENT=$workspace_name\n";
	close $p4_ini_file_handle;

	# make sure Perforce always looks for that file for workspace info
	p4_command("set P4CONFIG=p4.ini");

	# make sure the client exists
	@clients = p4_command("clients -e $workspace_name");
	fail("Failed to create client $workspace_name") if !grep(m/^Client $workspace_name_regex /, @clients);
	print "Completed in ".(time - $start_time)."s.\n";
	
	# return the new workspace info in a hash
	{ name => $workspace_name, stream => $stream, dir => $workspace_dir, metadata_dir => $workspace_metadata_dir, change_filename => $workspace_change_filename, capture_filename => $workspace_capture_filename, identifier => $workspace_identifier, incremental_sync => $workspace_incremental_sync, local_ddc => $workspace_local_ddc };
}

# clean the existing workspace. keep separate so we can do at the end of a build. note that this does not sync it; it just reconciles the have table.
sub clean_workspace
{
	my ($workspace) = @_;
	my $start_time = time;

	# get the path to the files describing the workspace state
	my $change_filename = $workspace->{'change_filename'};
	my $capture_filename = $workspace->{'capture_filename'};

	# check if there's an old capture file. if there is, we were aborted during an update and can restore it.
	if(-f "$change_filename.last" || -f "$capture_filename.last")
	{
		if(-f $change_filename && -f $capture_filename)
		{
			# new versions of both files already exist, but we didn't get around to deleting the old ones. delete them now.
			safe_delete_file("$change_filename.last");
			safe_delete_file("$capture_filename.last");
		}
		else
		{
			# we did not get as far as moving the new files into place. restore the old ones.
			safe_delete_file($change_filename);
			safe_delete_file($capture_filename);
			rename("$change_filename.last", $change_filename);
			rename("$capture_filename.last", $capture_filename);
		}
	}

	# before running a clean against the captured workspace settings, we need to ensure that p4's have table is consistent with our last full sync. if we
	# aborted during a previous sync, that will not be the case, and the reconcile operation may delete newly added files that we were in the middle of syncing.
	# ignore cases where this file does not exist for now, since the conform job may run on a branch with this code, but 
	if(-f $change_filename)
	{
		my $last_change = read_file($change_filename);
		if($last_change)
		{
			print "Syncing $workspace->{'name'} to last synced changelist ($last_change) prior to clean.\n";
			p4_command("-c$workspace->{'name'} sync -q $sync_arguments //...\@$last_change");
		}
	}

	# check we have a capture file. if it's missing, we'll assume the whole workspace may be invalid.
	if(-f $capture_filename)
	{
		# figure out which files need to be updated in the p4 have table
		my $outdated_filename = join_paths($workspace->{'metadata_dir'}, 'clean-outdated.txt');
		print "Cleaning $workspace->{'dir'}...\n";
		run_reconcile_workspace($workspace, "restore \"$workspace->{'dir'}\" \"$capture_filename\" $outdated_filename");
		
		# read the list of files
		open(my $outdated_file, '<', $outdated_filename) or fail("Failed to open $outdated_filename.");
		my @outdated_files = <$outdated_file>;
		close($outdated_file);

		# sync the files back to revision zero. they've already been deleted, so we can disable p4 file ops.
		my $sync_filename = join_paths($workspace->{'metadata_dir'}, 'clean-sync.txt');
		open(my $sync_file_handle, '>', $sync_filename) or fail("Failed to open $sync_filename.");
		foreach(@outdated_files)
		{
			# escape any special p4 characters in the path
			chomp;
			s/%/%25/g;
			s/@/%40/g;
			s/#/%23/g;
			s/\*/%2A/g;
			print $sync_file_handle "//$workspace->{'name'}$_#0\n";
		}
		close $sync_file_handle;
		p4_command("-c$workspace->{'name'} -x$sync_filename sync -q -k", {'ignore_not_in_client_view' => 1});
		
		# remove all the temporary files
		unlink($outdated_filename);
		unlink($sync_filename);
	}
	else
	{
		# Delete everything and sync to revision zero
		print "No workspace capture file at $capture_filename; resetting have table and deleting branch.\n";
		p4_command("-c$workspace->{'name'} sync -q -k //$workspace->{'name'}/...#0");
		safe_delete_directory_contents($workspace->{'metadata_dir'});
	}
	print "Completed in ".(time - $start_time)."s.\n";
}

# get a list of submitted changes
sub get_submitted_changes
{
	my ($workspace, $filter, $max_results) = @_;
	
	# parse out the change information
	my $changes = [];
	foreach(p4_command("-c$workspace->{'name'} changes -s submitted -t -m $max_results \"$filter\""))
	{
		if(/^Change (\d+) on ([\d+\/]+ [\d:]+) by ([^ ]+) '(.*)'$/)
		{
			push(@{$changes}, { change => $1, time => p4_parse_time($2), author => $3, summary => $4 });
		}
	}
	$changes;
}

# sync the workspace to a given changelist, and capture it so we can quickly clean in the future
sub sync_workspace
{
	my ($workspace, $optional_arguments) = @_;

	# clean the current workspace
	clean_workspace($workspace) unless $optional_arguments->{'already_clean'} || $workspace->{'incremental_sync'};

	# sync to the right changelist
	my $sync_start_time = time;
	my $estimate_text = "";
	my $change = $optional_arguments->{'change'};
	if(!defined $change)
	{
		# find the most recent change and use that
		my $build_changes = get_submitted_changes($workspace, "//$workspace->{'name'}/...", 1);
		fail("Could not find any submitted changes to //$workspace->{'name'}/...") if $#{$build_changes} < 0;
		$change = $build_changes->[0]->{'change'};
	}
	elsif($optional_arguments->{'ensure_recent'})
	{
		# find the last change before the build changelist
		my $build_changes = get_submitted_changes($workspace, "//$workspace->{'name'}/...\@$change", 1);
		fail("Could not find any submitted changes to stream before $change") if $#{$build_changes} < 0;
		my $build_change = $build_changes->[0];
		
		# find the last change submitted to the stream
		my $last_changes = get_submitted_changes($workspace, "//$workspace->{'name'}/...", 1);
		fail("Could not find any submitted changes to stream") if $#{$last_changes} < 0;
		my $last_change = $last_changes->[0];

		# check it's recent
		if($build_change->{'time'} < $last_change->{'time'} - (30 * 24 * 60 * 60))
		{
			fail("Change being built (CL $build_change->{'change'}, ".format_date_time($build_change->{'time'}).") is over 30 days older than most recent change (CL $last_change->{'change'}, ".format_date_time($last_change->{'time'})."). Check that the build changelist is correct, or pass --allow-old-change.");
		}
	}
	if($optional_arguments->{'show_traffic'})
	{
		# calculate the amount of data we need to transfer. This incurs the same overhead as an actual sync (seems to be ~5s), so only do it if explicitly requested.
		my @estimates = p4_command("-c$workspace->{'name'} sync -N //...".(defined $change? "\@$change" : ""));
		if($#estimates >= 0 && $estimates[0] =~ m/files added\/updated\/deleted=([0-9]+)\/([0-9]+)\/([0-9]+), bytes added\/updated=([0-9]+)\/([0-9]+)/)
		{
			my $total_files = $1 + $2 + $3;
			my $total_size = int((($4 + $5) + (1024 * 1024) - 1) / (1024 * 1024));
			if($total_files > 0 || $total_size > 0)
			{
				$estimate_text = " ($total_files files, ${total_size}mb)";
			}
		}
	}
	print "Syncing $workspace->{'name'} to changelist $change"."$estimate_text.\n";
	p4_command("-c$workspace->{'name'} sync -q $sync_arguments //...\@$change");
	print "Completed in ".(time - $sync_start_time)."s\n";

	# capture the new state.
	my $capture_start_time = time;
	if(!$workspace->{'incremental_sync'})
	{
		# save the cl we've synced to, so we can safely restore it if interrupted. we need to be careful creating a file containing
		# the CL if we're conforming from 
		my $change_filename = $workspace->{'change_filename'};
		if(!$optional_arguments->{'no_new_change_file'} || -f $change_filename)
		{
			write_file("$change_filename.next", "$change");
		}

		# capture the new workspace contents
		my $capture_filename = $workspace->{'capture_filename'};
		print "Capturing workspace state to $capture_filename\n";
		run_reconcile_workspace($workspace, "capture \"$workspace->{'dir'}\" \"$capture_filename.next\"");

		# we're careful to execute things in the right order to be tolerant of interruptions here (eg. the job being aborted). Each of the following file system operations
		# is atomic, so we can resume from any intermediate state in clean_workspace. First we rename the existing file, then we move the new file into place, then we 
		# remove the backup of the original file.
		rename $change_filename, "$change_filename.last" if -f $change_filename;
		rename $capture_filename, "$capture_filename.last" if -f $capture_filename;
		rename "$change_filename.next", $change_filename if -f "$change_filename.next";
		rename "$capture_filename.next", $capture_filename;
		safe_delete_file("$change_filename.last") if -f "$change_filename.last";
		safe_delete_file("$capture_filename.last") if -f "$capture_filename.last";
	}

	# print the timing info for the capture
	print "Completed in ".(time - $capture_start_time)."s\n\n";
	
	# if we're running a preflight
	my $unshelve_change = $optional_arguments->{'unshelve_change'};
	if($unshelve_change)
	{
		fail("Cannot unshelve files into incrementally synced workspace") if $workspace->{'incremental_sync'};
		fake_unshelve_files($workspace, $unshelve_change);
	}
}

# replays the effects of unshelving a changelist, but clobbering files in the workspace rather than actually unshelving them (to prevent problems with multiple machines locking them)
sub fake_unshelve_files
{
	my ($workspace, $unshelve_changelist) = @_;
	print "Unshelving changelist $unshelve_changelist...\n";
	my $start_time = time;
	
	# query the contents of the shelved changelist
	my @records = p4_tagged_command("describe -s -S $unshelve_changelist");
	my $last_record = $records[$#records];
	fail("Changelist $unshelve_changelist is not shelved.\n") if !defined $last_record->{'shelved'};
	
	# parse out all the list of deleted and modified files
	my @delete_files = ();
	my @write_files = ();
	for(my $file_idx = 0;;$file_idx++)
	{
		my $depot_file = $last_record->{"depotFile$file_idx"};
		my $action = $last_record->{"action$file_idx"};
		last if !defined $depot_file || !defined $action;
		
		if($action eq 'delete' || $action eq 'move/delete')
		{
			push(@delete_files, $depot_file);
		}
		elsif($action eq 'add' || $action eq 'edit' || $action eq 'move/add' || $action eq 'branch' || $action eq 'integrate')
		{
			push(@write_files, $depot_file);
		}
		else
		{
			fail("Unknown action for shelved file '$action': $depot_file");
		}
	}

	# delete all the files
	foreach my $delete_file (@delete_files)
	{
		my $local_path = p4_get_local_path($workspace, $delete_file);
		if(defined $local_path && -f $local_path)
		{
			print "  Deleting: $local_path\n";
			safe_delete_file($local_path);
		}
	}
	
	# add all the new files
	foreach my $write_file (@write_files)
	{
		my $local_path = p4_get_local_path($workspace, $write_file);
		if($local_path)
		{
			print "  Writing: $local_path\n";
			p4_command("print -o \"$local_path\" \"$write_file@=$unshelve_changelist\"");
		}
	}
	print "Completed in ".(time - $start_time)."s\n";
}

# sync and run the ReconcileWorkspace utility with the given arguments
sub run_reconcile_workspace
{
	my ($workspace, $arguments) = @_;

	# use the print command to get the latest version of the executable without needing to sync it. uniquify the filename so it doesn't cause sharing violations if run by multiple jobsteps simultaneously.
	my $reconcile_workspace_filename = "ReconcileWorkspace+$workspace->{'name'}.exe";
	if(!-f $reconcile_workspace_filename)
	{
		my $reconcile_workspace_syncpath = "//$workspace->{'name'}/Engine/Extras/UnsupportedTools/ReconcileWorkspace/bin/release/ReconcileWorkspace.exe";
		p4_command("-c$workspace->{'name'} print -o $reconcile_workspace_filename $reconcile_workspace_syncpath");
		fail("Failed to sync $reconcile_workspace_syncpath") if !-f $reconcile_workspace_filename;
	}
	
	# spawn it with the given arguments.
	my $result = run_dotnet("$reconcile_workspace_filename $arguments");
	$result == 0 or fail("ReconcileWorkspace failed ($result)");
}

# sync and run the ReconcileWorkspace utility on the given directory
sub terminate_processes
{
	my ($workspace, $directory) = @_;

	if(is_windows())
	{
		# use the print command to get the latest version of the executable without needing to sync it. uniquify the filename so it doesn't cause sharing violations if run by multiple jobsteps simultaneously.
		my $terminate_processes_filename = "TerminateProcesses+$workspace->{'name'}.exe";
		if(!-f $terminate_processes_filename)
		{
			my $terminate_processes_syncpath = "//$workspace->{'name'}/Engine/Extras/UnsupportedTools/Windows/TerminateProcesses/Release/TerminateProcesses.exe";
			p4_command("-c$workspace->{'name'} print -o $terminate_processes_filename $terminate_processes_syncpath");
			fail("Failed to sync $terminate_processes_syncpath") if !-f $terminate_processes_filename;
		}
	
		# spawn it with the given arguments.
		system("$terminate_processes_filename -Directory=\"$directory\" -Image=orbis-tm.exe -Image=mspdbsrv.exe");
		$? == 0 or fail("TerminateProcesses failed ($?)");
	}
}

# reads the settings for a stream, caching them to the working directory
sub read_stream_settings
{
	my ($ec, $ec_project, $stream) = @_;

	# get the escaped stream name
	my $escaped_stream = $stream;
	$escaped_stream =~ s/\//+/g;

	# check if we already have the settings cached for this stream
	my $cache_location = "$escaped_stream.json";
	if(!$ENV{'COMMANDER_WORKSPACE'} || !-f $cache_location)
	{
		# allow the settings to be a direct property, or nested under a property sheet in a property called 'Settings'
		my $settings_path = ec_get_property($ec, "/projects[$ec_project]/Streams/$escaped_stream");
		if(!$settings_path)
		{
			$settings_path = ec_get_property($ec, "/projects[$ec_project]/Streams/$escaped_stream/Settings");
		}
		
		# if we're running outside of EC, we can try reading the file directly from the local machine, making it easier to iterate.
		if(!$ENV{'COMMANDER_WORKSPACE'})
		{
			my @records = p4_tagged_command("where $settings_path", { errors_as_warnings => 1 });
			if($#records == 0)
			{
				$cache_location = $records[0]->{'path'};
				print "Using local stream settings from $cache_location\n";
			}
		}

		# if it still doesn't exist, print it out from p4
		if(!-f $cache_location)
		{
			p4_command("print -q -o \"$cache_location\" \"$settings_path\"");
		}
	}
		
	# return the json blob
	read_json($cache_location);
}

# reads the settings for a stream, caching them to the working directory
sub read_all_stream_settings
{
	my ($ec, $ec_project) = @_;

	my $all_streams = {};
	my $all_streams_file = 'AllStreams.json';
	if(-f $all_streams_file)
	{
		$all_streams = read_json($all_streams_file);
	}
	else
	{
		# cache all the stream settings, so we don't have race conditions between each of the builders 
		my $streams = ec_get_property_sheet($ec, { path => "/projects[$ec_project]/Streams", recurse => 1 });
		foreach my $escaped_stream (keys %{$streams})
		{
			# allow the settings to be a direct property, or nested under a property sheet in a property called 'Settings'
			my $settings_path = $streams->{$escaped_stream};
			if($settings_path && ref $settings_path)
			{
				$settings_path = $settings_path->{'Settings'};
			}
			if($settings_path)
			{
				# print the settings to the workspace
				my $cache_location = "$escaped_stream.json";
				p4_command("print -q -o \"$cache_location\" \"$settings_path\"");

				# un-escape the stream name
				my $stream = $escaped_stream;
				$stream =~ s/\+/\//g;
				
				# add it to the stream dictionary
				$all_streams->{$stream} = read_json($cache_location);
			}
		}
		write_json($all_streams_file, $all_streams);
	}
}

#### Commands ###############################################################################################################################################################################################################

# syncs all the build machines to a clean state
sub conform_resources
{
	my ($ec, $ec_project, $ec_job, $ec_jobstep, $input_names, $set_pools, $delete_folders, $p4_clean, $max_parallel, $ec_update) = @_;
	
	# find all of the actual resources, and whether they're alive or not
	my $response = $ec->getResources();

	# find all the resource names in the given pools
	my %unique_resource_names;
	foreach my $input_name (split /\s+/, $input_names)
	{
		my $resolved_name = 0;
		foreach my $resource_node ($response->findnodes('/responses/response/resource'))
		{
			my $name = $response->findvalue('resourceName', $resource_node)->string_value();
			my @pools = split /\s+/, $response->findvalue('pools', $resource_node)->string_value();
			if(lc $name eq lc $input_name || (grep { lc $_ eq lc $input_name } @pools))
			{
				if($response->findvalue('agentState/alive', $resource_node)->string_value())
				{
					$unique_resource_names{$name} = 1;
				}
				else
				{
					log_print("Not conforming $name; agent is reported as dead.\n");
				}
				$resolved_name = 1;
			}
		}
		fail("Couldn't find any matching resources for $input_name") if !$resolved_name;
	}
	
	# find all the default workspaces
	my %resource_name_to_default_workspace;
	foreach my $resource_node ($response->findnodes('/responses/response/resource'))
	{
		my $name = $response->findvalue('resourceName', $resource_node)->string_value();
		my $default_workspace = $response->findvalue('workspaceName', $resource_node)->string_value();
		$resource_name_to_default_workspace{$name} = $default_workspace;
	}
	
	# convert it to an array and sort
	my @resource_names = sort keys %unique_resource_names;
	log_print "Matching resources: ".join(", ", @resource_names)."\n";

	# cache all the stream settings, so we don't have race conditions between each of the builders 
	read_all_stream_settings($ec, $ec_project);
	
	# create a child jobstep for each one
	log_print("Creating child steps...");
	my $jobstep_arguments_array = [];
	for(my $idx = 0; $idx <= $#resource_names; $idx++)
	{
		my $resource_name = $resource_names[$idx];
	
		my $command = '';
		$command .= "standard_builder_setup('\$[/myResource/resourceName]');\n";
		$command .= "run_command('ConformResource --name=$resource_name".($set_pools? " --set-pools=\"$set_pools\"" : "").($delete_folders? " --delete-folders=\"$delete_folders\"" : "").($p4_clean? " --p4-clean" : "")." --num-slots=\$[/myResource/stepLimit] --ec-update');\n";
		$command .= "\n";
		$command .= "\$[/myProject/Functions/standard_builder_setup]\n";
		$command .= "\n";
		$command .= "\$[/myProject/Functions/run_command]\n";
	
		my $jobstep_arguments = {};
		$jobstep_arguments->{'jobStepId'} = $ec_jobstep || "?";
		$jobstep_arguments->{'jobStepName'} = "Conform $resource_name";
		$jobstep_arguments->{'projectName'} = $ec_project;
		$jobstep_arguments->{'parallel'} = '1';
		$jobstep_arguments->{'command'} = $command;
		$jobstep_arguments->{'shell'} = 'ec-perl';
		$jobstep_arguments->{'resourceName'} = $resource_name;
		$jobstep_arguments->{'workspaceName'} = $resource_name_to_default_workspace{$resource_name};
		$jobstep_arguments->{'postProcessor'} = 'ec-perl PostpFilter.pl | Postp --load=./PostpExtensions.pl';
		
		if($max_parallel && $idx >= $max_parallel)
		{
			my $previous_jobstep = "/jobs[".($ec_job || "?")."]/jobSteps[Conform Resources]/jobSteps[Conform $resource_names[$idx - $max_parallel]]";
			$jobstep_arguments->{'precondition'} = "\$[/javascript (getProperty('$previous_jobstep/status') == 'completed')? true : false]";
		}
	
		my $mac_credential_response = $ec->getFullCredential('Mac');
		foreach my $mac_credential ($mac_credential_response->findnodes("/responses/response/credential"))
		{
			my $username = $mac_credential_response->findvalue("./userName", $mac_credential)->string_value();
			my $password = $mac_credential_response->findvalue("./password", $mac_credential)->string_value();
			$jobstep_arguments->{'credential'} = [{ credentialName => 'Mac', userName => $username, password => $password }];
			last;
		}
		
		push(@{$jobstep_arguments_array}, $jobstep_arguments);
	}
	ec_create_jobsteps($ec, $jobstep_arguments_array, 'jobsteps.txt', $ec_update);
}

# returns an resource to a clean state; syncing and cleaning all branches, and removing unused branches
sub conform_resource
{
	my ($ec, $ec_project, $resource_name, $root_dir, $delete_folders, $p4_clean, $num_slots) = @_;
	
	# figure out which pools we belong to
	print "Querying resource pools for $resource_name...\n";
	my $resource = $ec->getResource($resource_name);
	my @resource_pools_array = split /\s+/, $resource->findvalue("//pools")->string_value();
	my %resource_pools = ($resource_name => 1);
	$resource_pools{$_} = 1 foreach @resource_pools_array;
	print "Pools: ".join(', ', @resource_pools_array)."\n\n";
	
	# delete all the requested folders on the machine before starting the conform
	if($delete_folders)
	{
		foreach my $delete_folder(split /;/, $delete_folders)
		{
			my $directory_name = join_paths($root_dir, $delete_folder);
			if(-d $directory_name)
			{
				print "Deleting directory: $directory_name\n";
				safe_delete_directory($directory_name);
				fail("Unable to delete '$directory_name'") if -d $directory_name;
			}
		}
	}
	
	# read settings for all the streams
	print "Querying workspaces for this agent...\n";
	my %stream_workspace_to_agent_type;
	my $all_streams = read_all_stream_settings($ec, $ec_project);
	foreach my $stream_key (keys %{$all_streams})
	{
		# get the settings for this stream
		my $settings = $all_streams->{$stream_key};

		# convert the key name to a stream
		my $stream = $stream_key;
		$stream =~ s/\+/\//g;

		# check to see if our pool is referenced by any agent types for this stream
		my $agent_types = $settings->{'Agent Types'};
		foreach my $agent_type(keys(%{$agent_types}))
		{
			# check if any pool for this resource makes it eligable to host this branch
			my $agent_settings = $agent_types->{$agent_type};
			my $agent_resource_pool = $agent_settings->{'Resource Pool'};
			next if !$resource_pools{$agent_resource_pool};

			# check it hasn't already been added
			my $workspace_name = $agent_settings->{'Workspace'} || 'Full';
			my $parallel = $agent_settings->{'Parallel'}? 1 : 0;
			my $stream_workspace = "$stream:$workspace_name:$parallel";
			next if defined $stream_workspace_to_agent_type{$stream_workspace};

			# add it
			print "$stream: $workspace_name\n";
			$stream_workspace_to_agent_type{$stream_workspace} = $agent_type;
		}
	}
	print "\n";
	
	# create the autosdk workspace
	my $autosdk_workspace = setup_autosdk_workspace($root_dir);
	print "\n";

	# make workspaces for all the unique configurations on this agent
	my @workspaces = ();
	foreach(keys(%stream_workspace_to_agent_type))
	{
		my ($stream, $is_parallel) = m/^([^:]+):.*:(0|1)/;
		my $agent_type = $stream_workspace_to_agent_type{$_};
		if($is_parallel)
		{
			push(@workspaces, setup_workspace($root_dir, $stream, { ec => $ec, ec_project => $ec_project, agent_type => $agent_type, slot_idx => $_ })) foreach(1..$num_slots);
		}
		else
		{
			push(@workspaces, setup_workspace($root_dir, $stream, { ec => $ec, ec_project => $ec_project, agent_type => $agent_type }));
		}
		print "\n";
	}

	# enumerate all the workspaces on this host, and check whether they match any that we're expecting
	my %p4_info = %{(p4_tagged_command('info -s'))[0]};
	my $user_name = $p4_info{'userName'};
	my $host_name = $p4_info{'clientHost'};
	my $client_prefix_pattern = quotemeta join_paths($root_dir, '');
	foreach(p4_tagged_command("clients -u $user_name"))
	{
		# check the host matches
		my $client_host = $_->{'Host'};
		next if lc $client_host ne lc $host_name;

		# check it's under the root
		my $client_root = $_->{'Root'};
		next if !($client_root =~ m/^$client_prefix_pattern/);

		# check it doesn't match one of the workspaces we're tracking
		my $client_name = $_->{'client'};
		my $client_is_in_use = (defined $autosdk_workspace && lc $autosdk_workspace->{'name'} eq lc $client_name);
		$client_is_in_use |= (lc $_->{'name'} eq lc $client_name) foreach(@workspaces);
		next if $client_is_in_use;

		# revert all the files in this clientspec, and delete it
		my $start_time = time;
		print "Deleting client $client_name...\n";
		p4_revert_all_changes($client_name);
		p4_command("client -d $client_name");
		print "Completed in ".(time - $start_time)."s.\n\n";
	}

	# delete all the directories which aren't a workspace root
	if(-d $root_dir)
	{
		print "Checking for workspaces to delete from this agent...\n";
		opendir(my $root_dir_handle, $root_dir);
		while(my $entry_name = readdir($root_dir_handle))
		{
			next if $entry_name eq '.' || $entry_name eq '..';

			my $full_name = join_paths($root_dir, $entry_name);
			if(-f $full_name)
			{
				# delete the file; there's not supposed to be anything in the root.
				safe_delete_file($full_name);
			}
			elsif(-d $full_name)
			{
				# check if it's the autosdk directory
				if(defined $autosdk_workspace && lc $autosdk_workspace->{'dir'} eq lc $full_name)
				{
					print "Skipping directory $full_name, used by $autosdk_workspace->{'name'}\n";
					last;
				}
			
				# check if it matches any known workspace, and skip if it does.
				my $is_known_workspace = 0;
				foreach(@workspaces)
				{
					if(lc $_->{'metadata_dir'} eq lc $full_name)
					{
						print "Skipping directory $full_name, used by $_->{'name'}\n";
						$is_known_workspace = 1;
						last;
					}
				}
				next if $is_known_workspace;

				# delete it
				print "Deleting directory $full_name\n";
				safe_delete_directory($full_name);
			}
		}
		closedir($root_dir_handle);
		print "\n";
	}

	# clean all the workspaces
	foreach my $workspace(@workspaces)
	{
		if(!$workspace->{'incremental_sync'})
		{
			clean_workspace($workspace);
		}
		if($p4_clean)
		{
			print "Running p4 clean for workspace $workspace->{'name'}...\n";
			my @output = p4_command("-c$workspace->{'name'} clean //$workspace->{'name'}/...", { ignore_no_files_to_reconcile => 1 });
			print "warning: $_\n" foreach(@output);
		}
		print "\n";
	}
	
	# sync the autosdk workspace
	if(defined $autosdk_workspace)
	{
		sync_autosdk_workspace($autosdk_workspace);
	}
	
	# sync the regular workspaces
	foreach(@workspaces)
	{
		sync_workspace($_, { already_clean => 1, no_new_change_file => 1 });
	}
}

1;