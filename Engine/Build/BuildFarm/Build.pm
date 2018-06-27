# Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

use strict;
use warnings;
use Data::Dumper;
use File::Copy;
use File::Path;
use File::Find;
use File::Spec;
use List::Util ('min', 'max');
use ElectricCommander();
use JSON;
use URI::Escape;
use Cwd;

# local modules
use Utility;
use Notifications;

########################################################################################################################################################################################################

# check if there have been any changes since the last CIS run
sub check_for_changes
{
	my ($ec, $ec_project, $ec_job, $stream, $ec_update) = @_;
	
	# read the stream settings
	my $settings = read_stream_settings($ec, $ec_project, $stream);
	my $build_types = $settings->{'Build Types'};

	# get the escaped stream name
	my $escaped_stream = $stream;
	$escaped_stream =~ s/\//+/g;
	
	# get the CIS state from electric commander
	my $cis_state_property = "/projects[$ec_project]/Generated/$escaped_stream/CIS";
	my $cis_state_json = ec_try_get_property($ec, $cis_state_property);
	my $cis_state = decode_json($cis_state_json || '{}');

	# make a lookup of name to build types
	my $name_to_build_type = {};
	foreach my $build_type(@{$build_types})
	{
		$name_to_build_type->{$build_type->{'Name'}} = $build_type;
	}

	# find all the assigned semaphores for jobs running in this branch. this will affect the jobs we're allowed to start.
	my $semaphores = {};

	my $filter = [];
	push(@{$filter}, { propertyName => 'projectName', operator => 'equals', operand1 => $ec_project });
	push(@{$filter}, { propertyName => 'procedureName', operator => 'equals', operand1 => 'Run BuildGraph' });
	push(@{$filter}, { propertyName => 'Stream', operator => 'equals', operand1 => $stream });
	push(@{$filter}, { propertyName => 'status', operator => 'notEqual', operand1 => 'completed' });

	my $jobs_response = $ec->findObjects('job', { maxIds => 100, filter => $filter, select => [{ propertyName => 'Semaphores'}] });
	foreach my $object ($jobs_response->findnodes("/responses/response/object"))
	{
		foreach my $property ($jobs_response->findnodes("property", $object))
		{
			my $name = $jobs_response->findvalue("propertyName", $property)->string_value();
			if($name eq 'Semaphores')
			{
				my $value = $jobs_response->findvalue("value", $property)->string_value();
				foreach my $semaphore_name(split /\s+/, $value)
				{
					$semaphores->{$semaphore_name}++;
				}
			}
		}
	}

	# larger builds can be a superset of smaller builds. expand out a list of builds covered by each type.
	my $name_to_included_names = { };
	foreach my $build_type(@{$build_types})
	{
		my @included_names = ($build_type->{'Name'});
		for(my $idx = 0; $idx <= $#included_names; $idx++)
		{
			my $includes = $name_to_build_type->{$included_names[$idx]}->{'Includes'};
			if($includes)
			{
				foreach my $include(split /;/, $includes)
				{
					push(@included_names, $include) if !grep { $_ eq $include } @included_names;
				}
			}
		}
		$name_to_included_names->{$included_names[0]} = \@included_names;
	}
	
	# get the current time to figure out whether to start a new build
	my $time = time;

	# create the new cis state object, making sure there's an entry for each name
	my $new_cis_state = {};
	foreach my $name(keys %{$name_to_build_type})
	{
		$new_cis_state->{$name}->{'CL'} = $cis_state->{$name}->{'CL'} || 0;
		$new_cis_state->{$name}->{'Time'} = $cis_state->{$name}->{'Time'} || $time;
	}

	# find the time value for midnight today.
	my $local_midnight = get_local_midnight($time);
	
	# create a cache for the latest change matching each filter
	my $filter_to_last_change = { };
	
	# figure out all the builds we can trigger
	my $new_builds = { };
	foreach my $build_type(@{$build_types})
	{
		# check if it's time to do a scheduled build
		my $schedule = $build_type->{'Schedule'};
		next if !$schedule;

		# get the filter for this build type
		my $filter = $build_type->{'Filter'} || '...';

		# find all the required semaphores to run this build
		my $have_required_semaphores = 1;
		foreach my $semaphore_name(split /\s+/, ($build_type->{'Semaphores'} || ''))
		{
			my $num_semaphores_taken = $semaphores->{$semaphore_name} || 0;
			if($num_semaphores_taken > 0)
			{
				$have_required_semaphores = 0;
				last;
			}
		}
		next if !$have_required_semaphores;

		# update the cache with the last change submitted that matches this filter, and the last change submitted by a non-buildmachine user
		my $last_change = $filter_to_last_change->{$filter}; 
		if(!$last_change)
		{
			# find the last changes submitted
			my $command = "changes -m 50";
			foreach(split /;/, $filter)
			{
				$command .= " \"$stream/$_\"";
			}
			foreach(p4_command($command))
			{
				fail("Unexpected output line when querying changes: $_") if !m/^Change (\d+) on [^ ]+ by ([^@]+)@/;
				$last_change->{'by_any'} = $1 if $1 > ($last_change->{'by_any'} || 0);
				$last_change->{'by_user'} = $1 if lc $2 ne 'buildmachine' && $1 > ($last_change->{'by_user'} || 0);
			}
			next if !$last_change || !$last_change->{'by_user'};
			$filter_to_last_change->{$filter} = $last_change;
		}
			
		# print the matching change info
		my $build_name = $build_type->{'Name'};
		print "$build_name:\n";
		print "    Last change matching filter '$filter' is $last_change->{'by_any'}".(($last_change->{'by_any'} != $last_change->{'by_user'})? " ($last_change->{'by_user'} excluding buildmachine)" : "")."\n";

		# make sure something has been submitted since the last build that ran
		if($new_cis_state->{$build_name}->{'CL'} >= $last_change->{'by_user'})
		{
			if(!exists $build_type->{'RequireSubmittedChange'} || $build_type->{'RequireSubmittedChange'})
			{
				print "    Already ran at CL $new_cis_state->{$build_name}->{'CL'}.\n";
				next;
			}
		}
			
		# figure out a reference time and interval for the given schedule
		my ($reference_time, $interval, $days_of_week);
		if($schedule =~ /^\s*Daily\s+At\s+(\d+):(\d\d)\s*$/i)
		{
			# specific time every day
			$reference_time = $local_midnight + (($1 * 60) + $2) * 60;
			$interval = 24 * 60 * 60;
		}
		elsif($schedule =~ /^\s*Daily\s+Except\s+(.*)\s+At\s+(\d+):(\d\d)\s*$/i)
		{
			# default to all days of the week, then remove the exceptions
			$days_of_week = { 0 => 1, 1 => 1, 2 => 1, 3 => 1, 4 => 1, 5 => 1, 6 => 1, 7 => 1 };
			delete($days_of_week->{$_}) foreach(keys %{parse_day_list($1)});

			# specific time every day
			$reference_time = $local_midnight + (($2 * 60) + $3) * 60;
			$interval = 24 * 60 * 60;
		}
		elsif($schedule =~ /^\s*(?:Every\s*)?(.+)\s+At\s+(\d+):(\d\d)\s*$/i)
		{
			# specific days
			$days_of_week = parse_day_list($1);
			$reference_time = $local_midnight + (($2 * 60) + $3) * 60;
			$interval = 24 * 60 * 60;
		}
		elsif($schedule =~ /^\s*Every\s+([^ ]*)\s*$/i)
		{
			# every X minutes throughout the day
			$reference_time = $local_midnight;
			$interval = parse_time_interval($1);
		}
		elsif($schedule =~ /^\s*Every\s+([^ ]+)\s+from\s+(\d+):(\d\d)\s*/i)
		{
			# every X minutes throughout the day starting at a certain time
			$reference_time = $local_midnight + (($2 * 60) + $3) * 60;
			$interval = parse_time_interval($1);
		}

		# check we got something valid
		if(!$reference_time || !$interval)
		{
			print "Warning: Couldn't parse schedule value '$schedule' for build type '$build_name'\n";
			next;
		}

		# find the next scheduled build time
		my $last_build_time = $new_cis_state->{$build_name}->{'Time'};
		my $next_build_time = $reference_time;
		if($days_of_week)
		{
			for(my $offset = 0; $offset < 7; $offset++)
			{
				my $wday = get_day_of_week($next_build_time);
				last if $days_of_week->{$wday};
				$next_build_time += $interval;
			}
		}
		$next_build_time += $interval while $next_build_time < $last_build_time;
	
		# check if we're past that time yet
		if($time < $next_build_time)
		{
			print "    Skipping build at CL $last_change->{'by_any'} until ".format_recent_time($next_build_time)."\n";
			next;
		}
		
		# ready to trigger
		print "    Ready to build CL $last_change->{'by_any'}.\n";
		$new_cis_state->{$build_name}->{'Time'} = $time;
		$new_builds->{$build_name} = $last_change->{'by_any'};
	}

	# update the time for every build type. we don't want to consider starting a new build again until the next interval elapses.
	foreach my $name(keys %{$name_to_build_type})
	{
		$new_cis_state->{$name}->{'Time'} = $time;
	}

	# remove everything that's already included as part of a larger build
	for my $new_build_name(keys %{$new_builds})
	{
		foreach my $included_name(@{$name_to_included_names->{$new_build_name}})
		{
			delete $new_builds->{$included_name} if $included_name ne $new_build_name;
		}
	}

	# update the CIS state
	for my $new_build_name(keys %{$new_builds})
	{
		foreach my $included_name(@{$name_to_included_names->{$new_build_name}})
		{
			$new_cis_state->{$included_name} = { 'CL' => $new_builds->{$new_build_name}, 'Time' => $time };
		}
	}
	
	# start all the new jobs
	print "\n";
	print "New builds: ".(join(", ", keys %{$new_builds}) || "none")."\n";
	if($ec_update)
	{
		# write the new state back to EC
		ec_set_property($ec, $cis_state_property, encode_json($new_cis_state), $ec_update);
	
		# start the jobs
		for my $new_build_name(keys %{$new_builds})
		{
			my $new_build_change = $new_builds->{$new_build_name};
			print "\n";
			print "Triggering $new_build_name at change $new_build_change.\n";
			
			# find the matching build type
			my $build_type = (grep { $_->{'Name'} eq $new_build_name } @{$build_types})[0];
			fail("Couldn't find build type for $new_build_name") if !$build_type;

			# create the new job
			my $actual_parameters = [];
			push(@{$actual_parameters}, { actualParameterName => 'Arguments', value => $build_type->{'Arguments'} });
			push(@{$actual_parameters}, { actualParameterName => 'Job Name', value => $new_build_name });
			push(@{$actual_parameters}, { actualParameterName => 'Stream', value => $stream });
			push(@{$actual_parameters}, { actualParameterName => 'CL', value => $new_build_change });
			push(@{$actual_parameters}, { actualParameterName => 'Semaphores', value => ($build_type->{'Semaphores'} || '') });
			push(@{$actual_parameters}, { actualParameterName => 'Initial Agent Type', value => $build_type->{'Initial Agent Type'} || "" });
			
			my $response = $ec->runProcedure($ec_project, { procedureName => 'Run BuildGraph', actualParameter => $actual_parameters });
			my $job_id = $response->findvalue("//response/jobId");
			fail("Couldn't create CIS job:\n".ec_get_response_string($response)) if !defined $job_id;
			
			# Add a link to the new job to the CIS page
			print "Started job $job_id\n";	
			my $safe_stream_name = $stream;
			$safe_stream_name =~ s/^\///g;
			$safe_stream_name =~ s/\// /g;
			ec_set_property($ec, "/jobs[$ec_job]/report-urls/Started $new_build_name for $safe_stream_name at CL $new_build_change", "/commander/link/jobDetails/jobs/$job_id?linkPageType=jobDetails", $ec_update);
		}
	}
}

# parse a string containing a list of days (eg. Monday, Wednesday and Saturday) into a hash containing keys for the indicies of those days of the week
sub parse_day_list
{
	my ($day_list_text) = @_;

	my @day_names = (split /\s+and\s+|,|\s/i, $day_list_text);
	print "Warning: Missing days from '$day_list_text'\n" if $#day_names == -1;

	my $day_index_lookup = { 'sunday' => 0, 'monday' => 1, 'tuesday' => 2, 'wednesday' => 3, 'thursday' => 4, 'friday' => 5, 'saturday' => 6 };

	my $day_list = {};
	foreach(@day_names)
	{
		if($_)
		{
			my $day_index = $day_index_lookup->{lc $_};
			if(defined $day_index)
			{
				$day_list->{$day_index} = 1;
			}
			else
			{
				print "Warning: Couldn't parse day name '$_'\n";
			}
		}
	}
	$day_list;
}

# runs uat to build the list of steps for this build 
sub build_setup
{
	my ($ec, $ec_project, $ec_jobstep, $workspace, $change, $code_change, $build_script, $target, $trigger, $token_signature, $temp_storage_dir, $pass_through_arguments, $optional_arguments, $ec_update) = @_;

	# build the command line	
	my $uat_arguments;
	$uat_arguments = "BuildGraph";
	$uat_arguments .= " -Script=".quote_argument($build_script);
	$uat_arguments .= " -Target=".quote_argument($target);
	$uat_arguments .= " -Export=".quote_argument(join_paths(getcwd(), "job.json"));
	$uat_arguments .= " -SharedStorageDir=".quote_argument($temp_storage_dir);
	$uat_arguments .= " -Trigger=".quote_argument($trigger) if $trigger;
	$uat_arguments .= " -TokenSignature=".quote_argument($token_signature);
	$uat_arguments .= " $pass_through_arguments" if $pass_through_arguments;
	$uat_arguments .= " CopyUAT -WithLauncher -TargetDir=".quote_argument(join_paths(getcwd(), "UAT"));

	# set up the environment variables for executing uat
	setup_uat_environment($workspace, $change, $code_change);
	
	# execute uat
	my $result = get_uat_exit_code($workspace->{'dir'}, $uat_arguments);
	
	# get the jobstep properties
	my $jobstep = ec_get_jobstep($ec, $ec_jobstep);

	# undefined variables for notifications
	my $job_definition = undef;
	my $repro_steps = undef;

	# get the notification settings
	my $notifications = get_jobstep_notifications($ec, $ec_project, $job_definition, $jobstep, $workspace->{'name'}, $workspace->{'dir'}, $repro_steps);
	if($notifications && $notifications->{'outcome'} ne 'success')
	{
		send_notifications($ec, $jobstep, $notifications, $optional_arguments->{'email_only'}, $ec_update);
	}

	# exit with the same result
	exit $result if $result != 0;
}

# configures the ec job from the job definition created by build_setup.
sub build_job_setup
{
	my ($ec, $ec_project, $ec_job, $ec_update, $settings, $workspace, $agent_type, $change, $build_script, $target, $token_signature, $shared_storage_block, $arguments, $pass_through_arguments, $optional_arguments) = @_;

	# Read the job definition
	my $job_definition = read_json('job.json');
	
	# If the first group is compatible with the current agent type, rename it to the startup group and save out the updated job definition. It
	# saves switching to a new agent to run the first items.
	my $group_definitions = $job_definition->{'Groups'};
	if($#{$group_definitions} >= 0)
	{
		foreach(@{$group_definitions->[0]->{'Agent Types'}})
		{
			if(lc $_ eq lc $agent_type)
			{
				$group_definitions->[0]->{'Name'} = 'Startup';
				write_json('job.json', $job_definition);
				last;
			}
		}
	}
	
	# build a mapping from node to jobstep path
	my $node_name_to_jobstep = get_node_jobstep_lookup($ec_job, $job_definition);

	# make a lookup of node name to its group
	my $node_name_to_group_name = get_node_to_group_lookup($job_definition);

	# make a lookup from node name to all the triggers that depend on it
	my $node_name_to_dependent_trigger_names = get_node_name_to_dependent_trigger_names_lookup($job_definition);
	
	# make a lookup of group names to the nodes 
	my $group_name_to_node_names = {};
	foreach my $node_name (keys %{$node_name_to_group_name})
	{
		my $group_name = $node_name_to_group_name->{$node_name};
		push @{$group_name_to_node_names->{$group_name}}, $node_name;
	}
	
	# make a lookup of node names to their dependencies
	my $node_name_to_dependencies = {};
	foreach my $group_definition (@{$group_definitions})
	{
		foreach my $node_definition (@{$group_definition->{'Nodes'}})
		{
			my $node_name = $node_definition->{'Name'};
			$node_name_to_dependencies->{$node_name} = $node_definition->{'DependsOn'};
		}
	}

	# build a list of nodes which are set to run early
	my $run_early_nodes = [];
	foreach my $group_definition (@{$job_definition->{'Groups'}})
	{
		foreach my $node_definition (@{$group_definition->{'Nodes'}})
		{
			push(@{$run_early_nodes}, $node_definition->{'Name'}) if $node_definition->{'RunEarly'};
		}
	}
	
	# update ec with info for the job, to be consumed by the CIS and build pages
	my $build_info = {};
	$build_info->{'NodesInJob'} = get_node_name_list($job_definition);
	$build_info->{'GroupToNodes'} = $group_name_to_node_names;
	$build_info->{'NodeToDependencies'} = $node_name_to_dependencies;
	$build_info->{'RunEarly'} = $run_early_nodes;
	ec_set_property($ec, "/jobs[$ec_job]/BuildInfo", encode_json($build_info), $ec_update);

	# get the agent definitions from the settings
	my $agent_type_definitions = $settings->{'Agent Types'};

	# create the command batch for the new EC jobsteps
	my @jobstep_arguments_array;
	
	# create procedures for all the jobsteps
	foreach my $group_definition (@{$group_definitions})
	{
		# get the current group name
		my $group_name = $group_definition->{'Name'};
		next if lc $group_name eq 'startup';

		# get the agent classes that this step can run on
		my $group_agent_types = $group_definition->{'Agent Types'};

		# find the agent type definition for this group, from the branch settings
		my $group_agent_type;
		my $group_agent_type_definition;
		foreach(@{$group_agent_types})
		{
			$group_agent_type = $_;
			$group_agent_type_definition = $agent_type_definitions->{$group_agent_type};
			last if defined $group_agent_type_definition;
		}
		fail("Couldn't find any matching agent for $group_name. Allowed: ".join(' ', @{$group_agent_types}).'. Available: '.join(', ', keys(%{$agent_type_definitions}))) if (!defined $group_agent_type_definition || !defined $group_agent_type);
	
		# get the corresponding resource pool
		my $group_resource_pool = $group_agent_type_definition->{'Resource Pool'};
		fail("Missing 'Resource Pool' setting for $group_agent_type in settings") if !defined $group_resource_pool;

		# check if there are nodes that can run early in this group
		my @run_early_nodes = grep($_->{'RunEarly'}, @{$group_definition->{'Nodes'}});

		# find all the dependencies that need to be met
		my %run_early_dependency_names = ();
		foreach my $run_early_node (@run_early_nodes)
		{
			foreach my $dependency_name (split /;/, $run_early_node->{'DependsOn'})
			{
				$run_early_dependency_names{$dependency_name} = 1;
			}
		}

		# find the dependencies of nodes in this group, arranged by the group they are in
		my %group_name_to_node_dependency_names = ();
		foreach my $node_definition (@{$group_definition->{'Nodes'}})
		{
			foreach my $node_dependency_name (split /;/, $node_definition->{'DependsOn'})
			{
				my $group_dependency_name = $node_name_to_group_name->{$node_dependency_name};
				if($group_dependency_name ne $group_name)
				{
					${$group_name_to_node_dependency_names{$group_dependency_name}}{$node_dependency_name} = 1;
				}
			}
		}
		
		# find all the triggers that depend on this group
		my %dependent_trigger_names = ();
		foreach my $node_definition (@{$group_definition->{'Nodes'}})
		{
			my $node_dependent_trigger_names = $node_name_to_dependent_trigger_names->{$node_definition->{'Name'}};
			if($node_dependent_trigger_names)
			{
				$dependent_trigger_names{$_} = 1 foreach @{$node_dependent_trigger_names};
			}
		}
	
		# create all the preconditions. we wait for all the external nodes to be complete, OR their parent procedure (in case its preconditions fail)
		my @preconditions = ();
		foreach my $group_dependency_name (keys %group_name_to_node_dependency_names)
		{
			my $node_dependency_names = $group_name_to_node_dependency_names{$group_dependency_name};

			# find the list of nodes we need to wait to complete
			my @node_completed_dependency_names;
			if($#run_early_nodes < 0)
			{
				@node_completed_dependency_names = keys %{$node_dependency_names};
			}
			else
			{
				@node_completed_dependency_names = grep($run_early_dependency_names{$_}, keys %{$node_dependency_names});
			}

			# if we don't need to wait for any completed nodes from this group, just wait until the group has started to be sure we don't get any deadlocks
			if($#node_completed_dependency_names < 0)
			{
				my $group_jobstep = get_group_jobstep($ec_job, $group_dependency_name);
				push(@preconditions, "(getProperty('$group_jobstep/status') == 'running' || getProperty('$group_jobstep/status') == 'completed')");
			}
			else
			{
				my @node_preconditions;
				foreach my $node_dependency_name (@node_completed_dependency_names)
				{
					my $jobstep = $node_name_to_jobstep->{$node_dependency_name};
					push(@node_preconditions, "getProperty('$jobstep/status') == 'completed'");
				}

				my $group_jobstep = get_group_jobstep($ec_job, $group_dependency_name);
				push(@preconditions, "(getProperty('$group_jobstep/status') == 'completed' || (".join(' && ', @node_preconditions).'))');
			}
		}

		# get the command line arguments for this group
		my $group_arguments = "--group=".quote_argument($group_name)." --agent-type=$group_agent_type --build-script=\"$build_script\"";
		$group_arguments .= " --token-signature=".quote_argument($token_signature);
		$group_arguments .= " --shared-storage-dir=".quote_argument(get_shared_storage_dir($workspace->{'stream'}, $settings, $group_agent_type, $shared_storage_block));
		foreach my $argument_key (keys(%{$arguments}))
		{
			if($argument_key ne 'node' && $argument_key ne 'resource-name' && $argument_key ne 'agent-type' && $argument_key ne 'temp-storage-block' && $argument_key ne 'shared-storage-block' && $argument_key ne 'build-script')
			{
				my $argument_value = $arguments->{$argument_key};
				$argument_value = "\"$argument_value\"" if $argument_value =~ /\s/;
				$group_arguments .= " --$argument_key=$argument_value";
			}
		}
		$group_arguments .= " -- $pass_through_arguments" if $pass_through_arguments;
		
		# get the procedure to run
		my $procedure_name = $group_agent_type_definition->{'Parallel'}? 'Build Parallel Setup' : 'Build Agent Setup';

		# get the arguments for the new procedure
		my $jobstep_arguments = {};
		$jobstep_arguments->{'parentPath'} = "/jobs[$ec_job]";
		$jobstep_arguments->{'jobStepName'} = $group_name;
		$jobstep_arguments->{'subprocedure'} = $procedure_name;
		$jobstep_arguments->{'parallel'} = '1';
		$jobstep_arguments->{'actualParameter'} = [{ actualParameterName => 'Arguments', value => $group_arguments }, { actualParameterName => 'Resource Pool', value => $group_resource_pool }, { actualParameterName => 'Dependent Triggers', value => join(' ', keys %dependent_trigger_names) }];
		$jobstep_arguments->{'precondition'} = format_javascript_condition(join(' && ', @preconditions)) if $#preconditions >= 0;
		push(@jobstep_arguments_array, $jobstep_arguments);
	}

	# sync the UGS database tool 
	my $badge_definitions = $job_definition->{'Badges'};
	if($#{$badge_definitions} >= 0)
	{
		# get the job details
		my $job = ec_get_job($ec, $ec_job);
	
		# use the print command to get the latest version of the executable without needing to sync it.
		my $post_badge_status_filename = "PostBadgeStatus.exe";
		my $post_badge_status_syncpath = "//$workspace->{'name'}/Engine/Source/Programs/UnrealGameSync/PostBadgeStatus/bin/Release/PostBadgeStatus.exe";
		p4_command("-c$workspace->{'name'} print -o \"$post_badge_status_filename\" \"$post_badge_status_syncpath\"");
		fail("Failed to sync $post_badge_status_syncpath") if !-f $post_badge_status_filename;
	
		# create all the badge update monitors
		foreach my $badge_definition (@{$badge_definitions})
		{
			my $badge_name = $badge_definition->{'Name'};
			my $badge_change = $badge_definition->{'Change'} || $change;
			
			# create all the preconditions. we wait for all the external nodes to be complete, OR their parent procedure (in case its preconditions fail)
			my @preconditions = ();
			foreach my $node_dependency_name (split /;/, $badge_definition->{'DirectDependencies'})
			{
				my $group_dependency_name = $node_name_to_group_name->{$node_dependency_name};
				push(@preconditions, "(getProperty('".get_group_jobstep($ec_job, $group_dependency_name)."/status') == 'completed' || getProperty('$node_name_to_jobstep->{$node_dependency_name}/status') == 'completed')");
			}

			# get the badge command line arguments
			my $arguments = "";
			$arguments .= " --stream=$workspace->{'stream'}";
			$arguments .= " --change=$badge_change";
			$arguments .= " --build-script=$build_script";
			$arguments .= " --badge-name=\"$badge_definition->{'Name'}\"";
			$arguments .= " --ec-update";

			# create the procedure
			my $jobstep_arguments = {};
			$jobstep_arguments->{'parentPath'} = "/jobs[$ec_job]";
			$jobstep_arguments->{'jobStepName'} = "Badge: $badge_definition->{'Name'}";
			$jobstep_arguments->{'subprocedure'} = 'Build Badge Setup';
			$jobstep_arguments->{'parallel'} = '1';
			$jobstep_arguments->{'actualParameter'} = { actualParameterName => 'Arguments', value => $arguments };
			$jobstep_arguments->{'precondition'} = format_javascript_condition(join(' && ', @preconditions)) if $#preconditions >= 0;
			push(@jobstep_arguments_array, $jobstep_arguments);
			
			# set the initial status
			my $url = ec_get_full_url($job->{'job_url'});
			set_badge_status($badge_definition, $badge_change, $job, "Starting", $settings, $url);
		}
	}
	
	# create the report procedures
	my $report_definitions = $job_definition->{'Reports'};
	foreach my $report_definition (@{$report_definitions})
	{
		my $report_name = $report_definition->{'Name'};
		
		# create all the preconditions. we wait for all the external nodes to be complete, OR their parent procedure (in case its preconditions fail)
		my @preconditions = ();
		foreach my $node_dependency_name (split /;/, $report_definition->{'DirectDependencies'})
		{
			my $group_dependency_name = $node_name_to_group_name->{$node_dependency_name};
			push(@preconditions, "(getProperty('".get_group_jobstep($ec_job, $group_dependency_name)."/status') == 'completed' || getProperty('$node_name_to_jobstep->{$node_dependency_name}/status') == 'completed')");
		}

		# get the report command line arguments
		my $arguments = "";
		$arguments .= " --stream=$workspace->{'stream'}";
		$arguments .= " --change=$change";
		$arguments .= " --build-script=$build_script";
		$arguments .= " --report-name=\"$report_definition->{'Name'}\"";
		$arguments .= " --shared-storage-block=\"$shared_storage_block\"";
		$arguments .= " --email-only=$optional_arguments->{'email_only'}" if $optional_arguments->{'email_only'};
		$arguments .= " --token-signature=".quote_argument($token_signature);
		$arguments .= " --ec-update";

		# create the procedure
		my $jobstep_arguments = {};
		$jobstep_arguments->{'parentPath'} = "/jobs[$ec_job]";
		$jobstep_arguments->{'jobStepName'} = ($report_definition->{'IsTrigger'}? "Trigger: " : "Report: ").$report_definition->{'Name'};
		$jobstep_arguments->{'subprocedure'} = 'Build Report Setup';
		$jobstep_arguments->{'parallel'} = '1';
		$jobstep_arguments->{'actualParameter'} = { actualParameterName => 'Arguments', value => $arguments };
		$jobstep_arguments->{'precondition'} = format_javascript_condition(join(' && ', @preconditions)) if $#preconditions >= 0;
		push(@jobstep_arguments_array, $jobstep_arguments);

		# if it's a trigger, add a link to search for all the steps in this trigger. build_agent_setup() sets the 'Dependent Triggers' property on each job step it creates.
		if($report_definition->{'IsTrigger'})
		{
			ec_set_property($ec, "/jobs[$ec_job]/report-urls/Trigger: $report_name (Waiting For Dependencies)", get_trigger_search_url($ec_job, $report_name), $ec_update);
		}
	}
	
	# create the new jobsteps
	ec_create_jobsteps($ec, \@jobstep_arguments_array, 'jobsteps.txt', $ec_update);
}

# create all the job steps for a gubp agent sharing group.
sub build_agent_setup
{
	my ($ec, $ec_job, $workspace, $change, $code_change, $build_script, $target, $group, $token_signature, $resource_name, $shared_storage_dir, $ec_update, $optional_arguments) = @_;

	my $start_time = time;
	print "Creating agent job steps...\n";
	
	# Read the job definition
	my $job_definition = read_json('job.json');

	# build a mapping from node to jobstep path
	my $node_name_to_jobstep = get_node_jobstep_lookup($ec_job, $job_definition);
	
	# make a lookup of node name to its group
	my $node_name_to_group_name = get_node_to_group_lookup($job_definition);
	
	# make a lookup from node name to the triggers that depend on them
	my $node_name_to_dependent_trigger_names = get_node_name_to_dependent_trigger_names_lookup($job_definition);
	
	# create all the jobsteps for this group
	foreach my $group_definition (@{$job_definition->{'Groups'}})
	{
		my $group_name = $group_definition->{'Name'};
		if(lc $group_name eq lc $group)
		{
			# get commands to setup the environment for running gubp
			my $environment_commands = "";
			if(!$optional_arguments->{'parallel'})
			{
				my $environment = get_uat_environment($workspace, $change, $code_change);
				foreach(keys %{$environment})
				{
					if(is_windows())
					{
						$environment_commands .= "set $_=$environment->{$_}\n";
					}
					else
					{
						$environment_commands .= "export $_=$environment->{$_}\n";
					}
				}
			}

			# array of all the jobstep creation commands in this group
			my @jobstep_arguments_array;
		
			# create a jobstep for each node
			my $last_node_name = undef;
			my $node_definitions = $group_definition->{'Nodes'};
			foreach my $node_definition (@{$node_definitions})
			{
				# get the node name
				my $node_name = $node_definition->{'Name'};

				# build lists of all the preconditions and runconditions 
				my @preconditions = ();
				my @runconditions = ();
				foreach my $depends_on_node (split /;/, $node_definition->{'DependsOn'})
				{
					my $depends_on_group = $node_name_to_group_name->{$depends_on_node};
					my $depends_on_jobstep = $node_name_to_jobstep->{$depends_on_node};
					push(@preconditions, "(getProperty('".get_group_jobstep($ec_job, $depends_on_group)."/status') == 'completed' || getProperty('$depends_on_jobstep/status') == 'completed')");
					push(@runconditions, "(getProperty('$depends_on_jobstep/outcome') == 'success' || getProperty('$depends_on_jobstep/outcome') == 'warning')");
				}
				push(@preconditions, "getProperty('".get_node_jobstep($ec_job, $group_definition->{'Name'}, $last_node_name)."/status') == 'completed'") if defined $last_node_name && !$optional_arguments->{'parallel'};

				# get the shared arguments
				my $shared_arguments = "";
				$shared_arguments .= " --change=$change";
				$shared_arguments .= " --code-change=$code_change";
				$shared_arguments .= " --preflight-change=$optional_arguments->{'preflight_change'}" if $optional_arguments->{'preflight_change'};
				$shared_arguments .= " --build-script=".quote_argument($build_script);
				$shared_arguments .= " --node=".quote_argument($node_name);
				$shared_arguments .= " --token-signature=".quote_argument($token_signature);
				$shared_arguments .= " --fake-build" if $optional_arguments->{'fake_build'};
				$shared_arguments .= " --fake-fail=$optional_arguments->{'fake_fail'}" if $optional_arguments->{'fake_fail'};
				$shared_arguments .= " --ec-update" if $ec_update;
				$shared_arguments .= " --autosdk" if $optional_arguments->{'autosdk'};
				$shared_arguments .= " --email-only=$optional_arguments->{'email_only'}" if $optional_arguments->{'email_only'};
				$shared_arguments .= " --";
				$shared_arguments .= " -Target=".quote_argument($target);
				$shared_arguments .= " -SharedStorageDir=".quote_argument($shared_storage_dir);
				$shared_arguments .= " $optional_arguments->{'pass_through_arguments'}" if $optional_arguments->{'pass_through_arguments'};

				# get the command to run.
				my $command = "";
				if($optional_arguments->{'parallel'})
				{
					$command .= "standard_builder_setup('\$[/myResource/resourceName]');\n";
					$command .= "run_command('BuildParallelNode --stream=$optional_arguments->{'stream'} --agent-type=$optional_arguments->{'agent_type'} --resource-name=\$[/myJobStep/assignedResourceName] --num-slots=\$[/myResource/stepLimit] $shared_arguments');\n";
					$command .= "\n";
					$command .= "\$[/myProject/Functions/standard_builder_setup]\n";
					$command .= "\$[/myProject/Functions/run_command]\n";
				}
				else
				{
					$command .= $environment_commands;
					$command .= "ec-perl Main.pl BuildSingleNode";
					$command .= " --stream=$workspace->{'stream'}";
					$command .= " --workspace-name=".quote_argument($workspace->{'name'});
					$command .= " --workspace-dir=".quote_argument($workspace->{'dir'});
					$command .= " $shared_arguments";
				}

				# get any special arguments for postp
				my $postp_arguments = "";
				$postp_arguments .= " --no-warnings" if !$node_definition->{'Notify'}->{'Warnings'};

				# create the step
				my $jobstep_arguments = {};
				$jobstep_arguments->{'jobStepName'} = $node_name;
				$jobstep_arguments->{'parallel'} = 1;
				$jobstep_arguments->{'resourceName'} = $resource_name;
				$jobstep_arguments->{'command'} = $command;
				$jobstep_arguments->{'shell'} = 'ec-perl' if $optional_arguments->{'parallel'};
				$jobstep_arguments->{'postProcessor'} = "ec-perl PostpFilter.pl $postp_arguments| Postp --load=./PostpExtensions.pl";
				$jobstep_arguments->{'precondition'} = format_javascript_condition(@preconditions);
				$jobstep_arguments->{'condition'} = format_javascript_condition(@runconditions);
				push(@jobstep_arguments_array, $jobstep_arguments);
				
				# update the loop vars
				$last_node_name = $node_name;
			}
			
			# create the jobsteps
			my $response = ec_create_jobsteps($ec, \@jobstep_arguments_array, 'jobsteps-agent.txt', $ec_update);
			if($response)
			{
				# update all the job steps which have triggers dependent on them with the names of those triggers. this allows us to search for all the steps which a trigger depends on.
				foreach my $jobstep($response->findnodes("/responses/response/jobStep"))
				{
					my $name = $response->findvalue("stepName", $jobstep)->string_value();
					my $trigger_names = $node_name_to_dependent_trigger_names->{$name};
					if($trigger_names)
					{
						my $jobstep_id = $response->findvalue("jobStepId", $jobstep)->string_value();
						ec_set_property($ec, "/jobSteps[$jobstep_id]/Dependent Triggers", join(' ', @{$trigger_names}), $ec_update);
					}
				}
			}
			last;
		}
	}
	
	# clear out the list of dependent triggers for this particular job step - the child job steps we created are the actual depdendencies
	ec_set_property($ec, "/myCall/Dependent Triggers", "", $ec_update);
	print "Completed in ".(time - $start_time)."s\n\n";
}

# execute a buildgraph node
sub build_single_node
{
	my ($ec, $ec_project, $ec_job, $ec_jobstep, $stream, $change_number, $workspace_name, $workspace_dir, $build_script, $node, $token_signature, $ec_update, $optional_arguments) = @_;

	# print out the log folder we'll be using right at the start. it's often vital for follow-up debugging.
	my $target_log_folder = join_paths('UAT Logs', $node);

	# set the final output path for UAT too
	my $final_log_folder = join_paths((is_windows()? $ENV{'COMMANDER_WORKSPACE_WINUNC'} : undef) || $ENV{'COMMANDER_WORKSPACE'}, $target_log_folder);
	print "Logs will be copied to $final_log_folder\n";
	$ENV{'uebp_FinalLogFolder'} = $final_log_folder;
	
	# remove any existing telemetry file first
	my $telemetry_file = join_paths($workspace_dir, $target_log_folder, "Telemetry.json");
	if(-f $telemetry_file)
	{
		unlink $telemetry_file;
	}

	# clear out the existing log folder. 
	# for subsequent nodes on the same agent, or incremental workspaces, we don't want this to just get bigger and bigger
	my $log_folder = $ENV{'uebp_LogFolder'};
	if($log_folder)
	{
		safe_delete_directory($log_folder);
		mkdir($log_folder);
	}
	
	# clear out all the local settings
	delete_engine_user_settings();

	# set EC properties for this job
	ec_set_property($ec, '/myJobStep/Stream', $stream, $ec_update);
	ec_set_property($ec, '/myJobStep/CL', $change_number, $ec_update);

	# get the UAT command line
	my $command;
	$command = "BuildGraph";
	$command .= " -NoCompile";
	$command .= " -Script=".quote_argument($build_script);
	$command .= " -SingleNode=".quote_argument($node);
	$command .= " -TokenSignature=".quote_argument($token_signature);
	$command .= " $optional_arguments->{'pass_through_arguments'}" if $optional_arguments->{'pass_through_arguments'};
	$command .= " -Telemetry=\"$telemetry_file\"";

	# build the node
	my $result;
	if((defined $optional_arguments->{'fake_fail'}) && (grep { lc $_ eq lc $node } split(/\+/, $optional_arguments->{'fake_fail'})))
	{
		print "Forcing exit code to 1 due to --fake-fail option\n";
		$result = 1;
	}
	elsif($optional_arguments->{'fake_build'})
	{
		print "Skipping: '$command' due to --fake-build\n" if $optional_arguments->{'fake_build'};
		$result = 0;
	}
	else
	{
		# actually run UAT
		$result = get_uat_exit_code($workspace_dir, $command);
	}

	# copy all the logs to the local folder, so that they're preserved on the network
	if($log_folder)
	{
		mkdir($target_log_folder);
		copy_recursive($log_folder, $target_log_folder);
	}

	# update the telemetry for this jobstep
	if(-f $telemetry_file)
	{
		my $json = read_json($telemetry_file);
		if($json->{'Samples'} && $#{$json->{'Samples'}} >= 0)
		{
			ec_set_property($ec, "/myJobStep/Telemetry", encode_json($json), $ec_update);
		}
	}
	
	# read the job definition
	my $job_definition = read_json('job.json');
	
	# get the jobstep properties
	my $jobstep = ec_get_jobstep($ec, $ec_jobstep);

	# create the command line to reproduce this build locally
	my $repro_steps = "";
	$repro_steps .= is_windows()? "Engine\\Build\\BatchFiles\\RunUAT.bat" : "Engine/Build/BatchFiles/RunUAT.sh";
	$repro_steps .= " BuildGraph -Script=".quote_argument($build_script)." -Target=".quote_argument($node)." -P4";

	# get the notification settings
	my $notifications = get_jobstep_notifications($ec, $ec_project, $job_definition, $jobstep, $workspace_name, $workspace_dir, $repro_steps);
	if($notifications)
	{
		send_notifications($ec, $jobstep, $notifications, $optional_arguments->{'email_only'}, $ec_update);
	}

	# update the latest build in EC, unless we're just doing a build for one person (eg. preflight)
	if(!$optional_arguments->{'email_only'})
	{
		set_latest_build($ec, $ec_project, $stream, $change_number, $jobstep, $notifications, $ec_update);
	}
	
	# if UAT failed, exit with an error code of 1. the result of calling system() is actually the exit code shifted left 8 bits, 
	# which gets truncated to a byte (giving a value of zero!) if we return it directly.
	exit ($result? 1 : 0);
}

# validates the list of email addresses, and prints a warning for those that don't pass.
sub validate_emails
{
	my ($emails) = @_;

	# validating email addresses with regexes is a classic bad idea, but EC craps out if they don't match a simple form.
	my $filtered_emails = [];
	foreach(@{$emails})
	{
		if(/^[a-zA-Z0-9-]+(\.[a-zA-Z0-9-]+)*@[a-zA-Z0-9-]+(\.[a-zA-Z0-9-]+)+$/)
		{
			push(@{$filtered_emails}, $_);
		}
		else
		{
			print "Warning: Invalid email address in recipients list - ignored: $_\n";
		}
	}
	
	$filtered_emails;
}

# gets a chunk of the to or cc list for a notification email to ensure it doesn't exceed the max length allowed by EC
sub get_recipients_chunk
{
	my ($recipients) = @_;

	my $length = 0;

	my $recipients_chunk = [];
	foreach(@{$recipients})
	{
		$length += length($_) + 2;
		last if $length > 3000;
		push(@{$recipients_chunk}, $_);
	}
	$recipients_chunk;
}

# send a notification email 
sub send_notifications
{
	my ($ec, $jobstep, $notifications, $email_only, $ec_update) = @_;

	# get the standard recipients
	my $to_recipients = $notifications->{'fail_causer_emails'} || [];
	my $cc_recipients = $notifications->{'default_recipients'} || [];
	($to_recipients, $cc_recipients) = ($cc_recipients, []) if $#{$to_recipients} < 0;
		
	# allow overriding the recipients list with the --email-only parameter
	if($email_only)
	{
		$to_recipients = [ $email_only ];
		$cc_recipients = [];
	}

	# also include whoever started the job
	my $job = ec_get_job($ec, $jobstep->{'job_id'});
	if($job->{'started_by'} and $job->{'started_by'} ne "project: GUBP_V5" and $job->{'started_by'} ne "buildmachine")
	{
		push(@{$to_recipients}, $job->{'started_by'});
	}

	# get the default subject
	my $subject = "[Build] $jobstep->{'job_name'}";
#	my $subject = "[Build] $job->{'properties'}->{'Stream'} - $jobstep->{'jobstep_name'} - After $notifications->{'last_successful_change'}";
#	if($job->{'started_by'})
#	{
#		$subject = "[Build] $jobstep->{'job_name'}";
#	}

	# validate the emails
	$to_recipients = validate_emails($to_recipients);
	$cc_recipients = validate_emails($cc_recipients);

	# print the notification info
	while($#{$to_recipients} >= 0 || $#{$cc_recipients} >= 0)
	{
		my $to_recipients_chunk = get_recipients_chunk($to_recipients);
		my $cc_recipients_chunk = get_recipients_chunk($cc_recipients);

		my $to_recipients_list = join(", ", @{$to_recipients_chunk}) if $#{$to_recipients_chunk} >= 0;
		my $cc_recipients_list = join(", ", @{$cc_recipients_chunk}) if $#{$cc_recipients_chunk} >= 0;
		print "Sending notification to $to_recipients_list".($cc_recipients_list? " (cc: $cc_recipients_list)" : "")."\n";
			
		my $arguments = {};
		$arguments->{'configName'} = 'EpicMailer';
		$arguments->{'subject'} = $subject;
		$arguments->{'to'} = $to_recipients_chunk if $#{$to_recipients_chunk} >= 0;
		$arguments->{'cc'} = $cc_recipients_chunk if $#{$cc_recipients_chunk} >= 0;
		$arguments->{'html'} = $notifications->{'message_body'};
		if($ec_update)
		{
			eval { $ec->sendEmail($arguments); };
			if ($@) 
			{
				warn "Email failed to send: [$@]\n";
			}
		}
		else
		{
			print "Skipping send due to missing --ec-update\n";
		}

		$to_recipients = [splice @{$to_recipients}, $#{$to_recipients_chunk} + 1] if $#{$to_recipients_chunk} >= 0;
		$cc_recipients = [splice @{$cc_recipients}, $#{$cc_recipients_chunk} + 1] if $#{$cc_recipients_chunk} >= 0;
	}
}

# clears all user engine settings
sub delete_engine_user_settings
{
	# clean up the local preferences
	if(is_windows())
	{
		my $appdata_dir = $ENV{'LOCALAPPDATA'};
		if($appdata_dir)
		{
			my @dir_names = ( "Unreal Engine", "UnrealEngine", "UnrealEngineLauncher", "UnrealHeaderTool", "UnrealPak" );
			foreach my $dir_name(@dir_names)
			{
				my $settings_dir = "$appdata_dir\\$dir_name";
				if($settings_dir && -d $settings_dir)
				{
					print "Removing local settings directory ($settings_dir)...\n";
					safe_delete_directory($settings_dir);
				}
			}
		}
	}
	else
	{
		my $home_dir = $ENV{'HOME'};
		if($home_dir)
		{
			my @dir_names = ( "Library/Preferences/Unreal Engine", "Library/Application Support/Epic" );
			foreach my $dir_name(@dir_names)
			{
				my $settings_dir = "$home_dir/$dir_name";
				if($settings_dir && -d $settings_dir)
				{
					print "Removing local settings directory ($settings_dir)...\n";
					safe_delete_directory($settings_dir);
				}
			}
		}
	}
}

# update the latest build property for a given node
sub set_latest_build
{
	my ($ec, $ec_project, $stream, $change, $jobstep, $notifications, $ec_update) = @_;

	my $latest_path = "/projects[$ec_project]/Generated/".escape_stream_name($stream)."/Latest/$jobstep->{'jobstep_name'}";

	# check there's not already a newer build present
	my $latest_json = $ec->evalScript("getProperty(\"$latest_path\")")->findvalue("//value")->string_value();
	if($latest_json)
	{
		my $latest = decode_json($latest_json);
		if($latest->{'change'} && $latest->{'change'} > $change)
		{
			return;
		}
	}

	# parse the notification settings
	my $outcome = ($notifications && $notifications->{'outcome'}) || 'success';
	my $submitters = ($notifications && $notifications->{'submitters'}) || [];
	my $muted_submitters = ($notifications && $notifications->{'muted_submitters'}) || [];
	my $last_successful_change = ($notifications && $notifications->{'last_successful_change'}) || $change;

	# create the new object
	$latest_json = "{ \"change\": $change, \"job_id\": $jobstep->{'job_id'}, \"job_name\": \"$jobstep->{'job_name'}\", \"jobstep_id\": $jobstep->{'jobstep_id'}, \"jobstep_name\": \"$jobstep->{'jobstep_name'}\", \"outcome\": \"$outcome\", \"time\": ".time.", \"last_successful_change\": $last_successful_change, \"submitters\": [ ".join(", ", map { "\"$_\"" } @{$submitters})." ], \"muted_submitters\": [ ".join(", ", map { "\"$_\"" } @{$muted_submitters})." ] }";
	ec_set_property($ec, $latest_path, $latest_json, $ec_update);
}

# evaluate the conditions for a trigger
sub build_report_setup
{
	my ($ec, $ec_job, $stream, $change, $report_name, $token_signature, $temp_storage_block, $ec_update, $optional_arguments) = @_;

	# get the job details
	my $job = ec_get_job($ec, $ec_job);

	# read the job definition
	my $job_definition = read_json('job.json');
	
	# find the trigger definition
	my $report_definition = find_report_definition($job_definition, $report_name);

	# get all the jobsteps for it
	my $jobsteps = get_dependency_jobsteps($job, [ split /;/, $report_definition->{'AllDependencies'} ]);
	
	# get the overall result
	my $result = ec_get_combined_result($jobsteps);
	
	# print out the dependencies
	my $max_name_length = 20;
	$max_name_length = max((length $_->{'jobstep_name'}) + 1, $max_name_length) foreach @{$jobsteps};
	print "Dependent job steps:\n";
	print "\n";
	foreach my $jobstep (@{$jobsteps})
	{
		print sprintf "  %20d %-${max_name_length}s $jobstep->{'result'}\n", $jobstep->{'jobstep_id'}, $jobstep->{'jobstep_name'};
	}
	print "\n";

	# if it's a trigger, update the properties for it
	my $subject = "[Build] $job->{'job_name'}: $report_name";
	if($report_definition->{'IsTrigger'})
	{
		# include the pass/fail state in the subject line
		$subject = $result? "[Trigger Ready] $subject" : "[Trigger Failure] $subject";
	
		# create the link to start the trigger
		if($result)
		{
			my $trigger_url = get_trigger_url($ec, $ec_job, $change, $token_signature, $temp_storage_block, $report_name);
			ec_set_property($ec, "/myJob/report-urls/Trigger: $report_name", $trigger_url, $ec_update);
			print "Trigger is ready to run ($trigger_url)\n";
		}
		else
		{
			my $trigger_search_url = 
			ec_set_property($ec, "/myJob/report-urls/Trigger: $report_name (Failed)", get_trigger_search_url($ec_job, $report_name), $ec_update);
			print "Cannot run trigger due to failures.";
		}

		# remove the existing link to search for all the pending job steps
		ec_delete_property($ec, "/myJob/report-urls/Trigger: $report_name (Waiting For Dependencies)", $ec_update);
	}

	# get a list of recipients for the notification
	my @to = ($optional_arguments->{'email_only'}) || @{split_list($report_definition->{'Notify'}, ';')};

	# if the job was manually started by someone, always include them
	my $started_by = $job->{'started_by'};
	push(@to, $started_by) if $started_by =~ /^[^ ]+\@[^ ]+$/;

	# send the notification email
	if($#to >= 0)
	{
	    # send the notification email
		my $arguments = {};
		$arguments->{'configName'} = 'EpicMailer';
		$arguments->{'subject'} = $subject;
		$arguments->{'to'} = [ @to ];
		$arguments->{'html'} = get_report_notification($report_definition, $job, $jobsteps);
		if($ec_update)
		{
			$ec->sendEmail($arguments);
		}
		else
		{
			print "Skipping send of email '$arguments->{subject}' to '".join("', '", @to)."'\n";
		}
	}

	# return the exit code, so this jobstep displays correctly in ec
	exit 1 if !$result;
}

# gets the url required to start a downstream job from the current one. copies all the same parameters (with the exception of the current trigger parameter)
sub get_trigger_url
{
	my ($ec, $ec_job, $change, $token_signature, $temp_storage_block, $trigger_name) = @_;
	
	# get the root parent job
	my $original_job = $ec_job;
	for(;;)
	{
		my $upstream_job = ec_try_get_property($ec, "/jobs[$original_job]/Upstream Job");
		last if !$upstream_job;
		$original_job = $upstream_job;
	}
	
	# get the details of the current job. the new trigger url will run the same procedure, with the same arguments, but include the name of the node 
	# to be triggered and the id of its parent job.
	my $jobinfo_response = $ec->getJobDetails($original_job);
	
	# find the first job object in the response. there could be zero or one.
	my $trigger_url;
	foreach my $jobinfo_object($jobinfo_response->findnodes("/responses/response/job"))
	{
		# create the trigger url
		$trigger_url = "/commander/link/runProcedure";
		$trigger_url .= "/projects/".encode_form_parameter($jobinfo_response->findvalue("./projectName", $jobinfo_object)->string_value());
		$trigger_url .= "/procedures/".encode_form_parameter($jobinfo_response->findvalue("./procedureName", $jobinfo_object)->string_value());
		$trigger_url .= "?runNow=0";
		$trigger_url .= "&priority=".encode_form_parameter($jobinfo_response->findvalue("./priority", $jobinfo_object)->string_value());
	
		# find all the parameters
		my $parameters = {};
		foreach my $parameter_object ($jobinfo_response->findnodes("./actualParameter", $jobinfo_object))
		{
			my $name = $jobinfo_response->findvalue("./actualParameterName", $parameter_object)->string_value();
			if($name)
			{
				$parameters->{$name} = $jobinfo_response->findvalue("./value", $parameter_object)->string_value();
			}
		}
	
		# insert the trigger parameters into the arguments
		$parameters->{'Arguments'} = "--trigger=\"$trigger_name\" --shared-storage-block=\"$temp_storage_block\" --token-signature=\"$token_signature\" $parameters->{'Arguments'}";
		
		# add the trigger name to the end of the build name
		$parameters->{'Job Name'} = "$parameters->{'Job Name'} - $trigger_name";

		# set the changelist number. this may have been set as a property after the job had started (overriding the parameter), so we need to fix it up now.
		$parameters->{'CL'} = $change;
		
		# set the upstream job to this one
		$parameters->{'Upstream Job'} = $ec_job;

		# append them all to the url
		my $parameter_idx = 1;
		foreach my $parameter_name (keys %{$parameters})
		{
			$trigger_url .= sprintf "&parameters%d_name=%s", $parameter_idx, encode_form_parameter($parameter_name);
			$trigger_url .= sprintf "&parameters%d_value=%s", $parameter_idx, encode_form_parameter($parameters->{$parameter_name});
			$parameter_idx++;
		}
		last;
	}
	$trigger_url;
}

# evaluate the conditions for a trigger
sub build_badge_setup
{
	my ($ec, $ec_project, $ec_job, $stream, $change, $badge_name, $ec_update) = @_;

	# get the job details
	my $job = ec_get_job($ec, $ec_job);
	
	# read the job definition
	my $job_definition = read_json('job.json');
	
	# find the badge definition
	my $badge_definition = find_badge_definition($job_definition, $badge_name) || fail("Couldn't find badge definition for $badge_name");

	# get all the dependent jobsteps
	my $jobsteps = get_dependency_jobsteps($job, [ split /;/, $badge_definition->{'AllDependencies'} ]);
	my $jobstep_urls = [];

	my $max_name_length = 20;
	$max_name_length = max((length $_->{'jobstep_name'}) + 1, $max_name_length) foreach @{$jobsteps};
	print "Dependent steps:\n";
	foreach my $jobstep (@{$jobsteps})
	{
		print sprintf "  %20d %-${max_name_length}s $jobstep->{'result'}\n", ($jobstep->{'jobstep_id'} || 0), $jobstep->{'jobstep_name'};
		push(@{$jobstep_urls}, ec_get_full_url($jobstep->{'jobstep_url'})) if $jobstep->{'result'} ne 'success' && defined $jobstep->{'jobstep_url'};
	}
	print "\n";
	
	# print out the status of each dependent job step in a format that allows the preprocessor to parse it
	my $has_errors = 0;
	my $has_warnings = 0;
	my $has_skipped = 0;
	foreach my $jobstep(@{$jobsteps})
	{
		my $result = $jobstep->{'result'};
		if($result ne 'success')
		{
			if($result eq 'warning')
			{
				$has_warnings = 1;
			}
			elsif($result eq 'skipped')
			{
				$has_skipped = 1;
			}
			else
			{
				$has_errors = 1;
			}
		}
	}
	
	# set the status of this jobstep to match
	if($ec_update)
	{
		if($has_errors)
		{
			$ec->setProperty("/myJobStep/outcome", "error");
		}
		elsif($has_warnings)
		{
			$ec->setProperty("/myJobStep/outcome", "warning");
		}
	}

	# get the url to display
	my $url;
	if($#{$jobstep_urls} == 0)
	{
		$url = $jobstep_urls->[0];
	}
	else
	{
		$url = ec_get_full_url($job->{'job_url'});
	}

	# get the settings for this branch
	my $settings = read_stream_settings($ec, $ec_project, $stream);
	my $status = $has_errors? "Failure" : $has_warnings? "Warning" : $has_skipped? "Skipped" : "Success";
	set_badge_status($badge_definition, $change, $job, $status, $settings, $url);
}

# update the status for a badge
sub set_badge_status
{
	my ($badge_definition, $change, $job, $status, $settings, $url) = @_;

	if($settings->{'Badge Url'})
	{
		my $command = "PostBadgeStatus.exe -Name=\"$badge_definition->{'Name'}\" -Change=$change -Project=\"$badge_definition->{'Project'}\" -RestUrl=\"$settings->{'Badge Url'}\" -Status=$status -Url=\"$url\"";
		print "Running $command...\n";
		system($command);
	}
}

# determines the temp storage block name from the job name
sub get_shared_storage_block
{
	my ($ec, $ec_job) = @_;
	
	my $job_info = $ec->getJobInfo($ec_job);
	my $job_name = $job_info->findvalue("/responses/response/job/jobName")->string_value();
	$job_name =~ / -\s+(.*)$/ or fail("Couldn't parse job name");
	
	$1;
}

# gets the complete temp storage directory from the agent settings and temp storage block
sub get_shared_storage_dir
{
	my ($stream, $settings, $agent_type, $shared_storage_block) = @_;

	my $shared_storage_root = $settings->{'Agent Types'}->{$agent_type}->{'Temp Storage'} || $settings->{'Agent Types'}->{$agent_type}->{'Shared Storage'};
	fail("Agent type '$agent_type' does not have a shared storage directory specified") if !$shared_storage_root;
	
	my $escaped_stream = $stream;
	$escaped_stream =~ s/\//+/g;

	my $path_separator = ($shared_storage_root =~ /\\/)? '\\' : '/';
	join($path_separator, ($shared_storage_root, $escaped_stream, $shared_storage_block));
}

# format a javascript ec condition
sub format_javascript_condition
{
	return ($#_ >= 0)? '$[/javascript if('.join(' && ', @_).') true;]' : 'true'
}

# get a list of all node names from the given job
sub get_node_name_list
{
	my ($job_definition) = @_;

	my $node_names = [];
	foreach my $group_definition (@{$job_definition->{'Groups'}})
	{
		foreach my $node_definition (@{$group_definition->{'Nodes'}})
		{
			my $node_name = $node_definition->{'Name'};
			push(@{$node_names}, $node_name);
		}
	}
	
	$node_names;
}

# make a lookup of group name to the corresponding jobstep
sub get_node_jobstep_lookup
{
	my ($ec_job, $job_definition) = @_;

	my %node_to_jobstep;
	foreach my $group_definition (@{$job_definition->{'Groups'}})
	{
		my $group_name = $group_definition->{'Name'};
		foreach my $node_definition (@{$group_definition->{'Nodes'}})
		{
			my $node_name = $node_definition->{'Name'};
			$node_to_jobstep{$node_name} = get_node_jobstep($ec_job, $group_name, $node_name);
		}
	}
	
	\%node_to_jobstep;
}

# make a lookup from node name to all the triggers that depend on it
sub get_node_name_to_dependent_trigger_names_lookup
{
	my ($job_definition) = @_;

	my $node_name_to_dependent_trigger_names = { };
	foreach my $trigger_definition (@{$job_definition->{'Triggers'}})
	{
		my $trigger_name = $trigger_definition->{'Name'};
		foreach my $node (split /;/, $trigger_definition->{'AllDependencies'})
		{
			push(@{$node_name_to_dependent_trigger_names->{$node}}, "'$trigger_name'");
		}
	}

	$node_name_to_dependent_trigger_names;
}

# make a lookup of node name to its corresponding group name
sub get_node_to_group_lookup
{
	my $job_definition = shift;

	my %node_to_group = ();
	foreach my $group_definition (@{$job_definition->{'Groups'}})
	{
		my $group_name = $group_definition->{'Name'};
		foreach my $node_definition (@{$group_definition->{'Nodes'}})
		{
			my $node_name = $node_definition->{'Name'};
			$node_to_group{$node_name} = $group_name;
		}
	}
	
	\%node_to_group;
}

# get the jobstep for a group procedure
sub get_group_jobstep
{
	my ($ec_job, $group_name) = @_;
	"/jobs[$ec_job]/jobSteps[$group_name]";
}

# get the jobstep for a single node
sub get_node_jobstep
{
	my ($ec_job, $group_name, $node_name) = @_;
	"/jobs[$ec_job]/jobSteps[$group_name]/jobSteps[Sync & Build]/jobSteps[$node_name]";
}

# gets the url of a search query to find all job steps for a trigger
sub get_trigger_search_url
{
	my ($ec_job, $trigger_name) = @_;
	
	my $search_url = "/commander/link/searchBuilder?formId=editSearch";
	$search_url .= "&objectType=jobStep";
	$search_url .= "&maxIds=200";
	$search_url .= "&maxResults=100";
	$search_url .= "&filtersJobStep1_intrinsic_name=jobId";
	$search_url .= "&filtersJobStep1_intrinsic_operator=equals";
	$search_url .= "&filtersJobStep1_intrinsic_operand1=$ec_job";
	$search_url .= "&filtersJobStep_intrinsic_last=2";
	$search_url .= "&filtersJobStep1_custom_name=Dependent+Triggers";
	$search_url .= "&filtersJobStep1_custom_operator=contains";
	$search_url .= "&filtersJobStep1_custom_operand1=".encode_form_parameter("'$trigger_name'");
	$search_url .= "&filtersJobStep_custom_last=2";
	$search_url .= "&sort1_name=createTime";
	$search_url .= "&sort1_order=ascending";
	$search_url .= "&sort_last=2";
	$search_url .= "&shortcutName=Job+Step+Search+Results";
	$search_url .= "&redirectTo=/commander/link/searchResults?filterName=jobStepSearch&reload=jobStep&s=JobSteps";
	
	$search_url;
}

# gets the UAT environment
sub get_uat_environment
{
	my ($workspace, $change, $code_change) = @_;

	my $build_root_escaped = $workspace->{'stream'};
	$build_root_escaped =~ s/\//+/g;

	my $environment = {};
	$environment->{'uebp_LOCAL_ROOT'} = $workspace->{'dir'};
	$environment->{'uebp_PORT'} = 'perforce.epicgames.net:1666';
	$environment->{'uebp_USER'} = 'buildmachine';
	$environment->{'uebp_CLIENT'} = $workspace->{'name'};
	$environment->{'uebp_BuildRoot_P4'} = $workspace->{'stream'};
	$environment->{'uebp_BuildRoot_Escaped'} = $build_root_escaped;
	$environment->{'uebp_CLIENT_ROOT'} = "//$workspace->{'name'}";
	$environment->{'uebp_CL'} = $change;
	$environment->{'uebp_CodeCL'} = $code_change;
	$environment->{'uebp_LogFolder'} = join_paths($workspace->{'dir'}, 'Engine', 'Programs', 'AutomationTool', 'Saved', 'Logs');
	$environment->{'P4USER'} = 'buildmachine';
	$environment->{'P4CLIENT'} = $workspace->{'name'};
	if(is_windows())
	{
		if($workspace->{'local_ddc'})
		{
			$environment->{'UE-LocalDataCachePath'} = $workspace->{'local_ddc'};
		}
		else
		{
			$environment->{'UE-LocalDataCachePath'} = "None";
		}
	}
	else
	{
		$environment->{'UE_SharedDataCachePath'} = '/Volumes/UE4DDC';
	}
	
	$environment;
}

# sets the UAT environment
sub setup_uat_environment
{
	my ($workspace, $change, $code_change) = @_;

	my $environment = get_uat_environment($workspace, $change, $code_change);
	foreach(keys %{$environment})
	{
		$ENV{$_} = $environment->{$_};
	}
}

# runs UAT with the given arguments
sub get_uat_exit_code
{
	my ($workspace_dir, $arguments) = @_;
	$arguments .= " -TimeStamps";

	my $initial_dir = cwd();
	my $runuat_script = is_windows()? "$workspace_dir\\Engine\\Build\\BatchFiles\\RunUAT.bat" : "$workspace_dir/Engine/Build/BatchFiles/RunUAT.sh";
	my $run_command = quote_argument($runuat_script)." $arguments";
	print "Running $run_command\n\n";

	my $result = system($run_command);

	print "\n";
	chdir $initial_dir;
	
	# kill any zombie ADB processes left over. ADB spawns a server which can keep a handle to stdout and prevent postp from terminating after a build finishes.
	if(is_windows())
	{
		system("cmd.exe /c taskkill /IM adb.exe /F /T 2>nul:");
	}
	
	$result;
}

1;