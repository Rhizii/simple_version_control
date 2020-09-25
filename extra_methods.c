#include "svc.h"



// frees all commits in the svc
void free_all_commits(helper* h)
{
  for (size_t i = 0; i < h->n_commits; i++)
  {
    free(h->all_commits[i]->message);
    free(h->all_commits[i]->changes);
    free(h->all_commits[i]->id);

    for (size_t j = 0; j < h->all_commits[i]->n_tracked; j++)
    {
      free(h->all_commits[i]->tracked[j].file_name);
    }

    free(h->all_commits[i]->tracked);
    free(h->all_commits[i]->branch);
    free(h->all_commits[i]);
  }
  free(h->all_commits);
}


void free_staged(helper* h)
{
  for (size_t i = 0; i < h->n_staged; i++)
  {
    free(h->staged[i]);
  }
  free(h->staged);
}

void free_branches(helper* h)
{
  for (size_t i = 0; i < h->n_branches; i++)
  {
    free(h->branches[i].name);
  }
  free(h->branches);
}



//* returns 1 if there have been changes betweeen the last commit's files and
// the current staged files.
//* If there are no changes , 0 is returned
int staged_has_changes(helper* h)
{
  // first commit case
  if (NULL == h->active_branch->head)
  {
    return 1;
  }

  if (h->active_branch->head->n_tracked != h->n_staged)
  {
    return 1;
  }
  else
  {
    // keeping track of how many of the files between the previous commit
    // and staged files have the same hash value
    size_t count = 0;
    for (size_t i = 0; i < h->n_staged; i++)
    {
      for (size_t j = 0; j < h->active_branch->head->n_tracked; j++)
      {
        if (hash_file(h, h->staged[i]) ==
            h->active_branch->head->tracked[j].hash)
        {
          count++;
        }
      }
    }

    if (count == h->n_staged)
    {
      return 0;
    }
    else
    {
      return 1;
    }
  }
  return 0;
}



// sets the 'type of change for each file in the commit
void set_changes(commit* previous, commit* new)
{

  // first commit case - all files are considered as 'additions'
  if (previous == NULL)
  {
    new->n_changes = new->n_tracked;
    new->changes = malloc(sizeof(struct change)*(new->n_changes));

    for (size_t i = 0; i < new->n_tracked; i++)
    {
      new->changes[i].file_name = new->tracked[i].file_name;
      new->changes[i].type = ADDED;
    }
    return;
  }


  new->n_changes = 0;
  int found_in_prev =  0;
  int not_modified = 0;

// checking new tracked files with previous tracked files to find
 // added and modified files
  for (size_t i = 0; i < new->n_tracked; i++)
  {
    found_in_prev = 0;
    not_modified = 0;

    for (size_t j = 0; j < previous->n_tracked; j++)
    {
      if (strcmp(new->tracked[i].file_name,
                previous->tracked[j].file_name) == 0)
      {
        found_in_prev = 1;

        // including file as a modified change if it is
        // found in the previous commit and the hashes of the file are the same
        if (new->tracked[i].hash == previous->tracked[j].hash)
        {
          not_modified = 1;
        }
        break;
      }

    }


    if (new->n_changes == 0)
    {
      new->changes = malloc(sizeof(struct change));
    }
    else
    {
      new->changes = realloc(new->changes, sizeof(struct change)*
                                                        (new->n_changes+2));
    }

    new->changes[new->n_changes].file_name = new->tracked[i].file_name;


    if (found_in_prev)
    {
      // including file as 'unchanged' if it is not modified
      if (not_modified)
      {
        new->changes[new->n_changes].type = UNCHANGED;
      }
      else
      {
        new->changes[new->n_changes].type = MODIFIED;
      }
    }
    // include file as an added change if it was not found
     // in the previous commit's tracked list
    else
    {
      new->changes[new->n_changes].type = ADDED;
    }

    new->n_changes++;
  }


// NOW checking tracked files in new commit with the tracked files in the
// previous commit to find REMOVED files
  int found_in_new = 0;
  for (size_t i = 0; i < previous->n_tracked; i++)
  {
    found_in_new = 0;

    for (size_t j = 0; j < new->n_tracked; j++)
    {
      if (strcmp(new->tracked[j].file_name,
                  previous->tracked[i].file_name) == 0)
      {
        found_in_new = 1;
        break;
      }
    }


    // add to removed list the file was not found in the
    // new commit's tracked list
    if (!found_in_new)
    {
      if (new->n_changes == 0)
      {
        new->changes = malloc(sizeof(struct change));
      }
      else
      {
        new->changes = realloc(new->changes, sizeof(struct change)*
                                                          (new->n_changes+2));
      }
      new->changes[new->n_changes].file_name = previous->tracked[i].file_name;
      new->changes[new->n_changes].type = REMOVED;
      new->n_changes++;
    }
  }
}

// comparator which organises changes alphbetically based on their file namess
int changes_cmp(const void* c1, const void* c2)
{
  return strcasecmp(((change*)c1)->file_name, ((change*)c2)->file_name);
}


// sets the commit of the commit based on the pseudocode algorithm
void set_commit_id(commit* commit)
{
  int id = 0;
  unsigned char byte;

  for (size_t i = 0; i < strlen(commit->message); i++)
  {
    byte = (unsigned char) commit->message[i];
    id = (id + byte) % 1000;
  }


  qsort(commit->changes, commit->n_changes, sizeof(struct change), changes_cmp);

  for (size_t i = 0; i < commit->n_changes; i++)
  {
    if (commit->changes[i].type == ADDED)
    {
      id += 376591;
    }
    else if (commit->changes[i].type == REMOVED)
    {
      id += 85973;
    }
    else if (commit->changes[i].type ==  MODIFIED)
    {
      id += 9573681;
    }

    if (commit->changes[i].type != UNCHANGED)
    {
      for (size_t j = 0; j < strlen(commit->changes[i].file_name); j++)
      {

        byte = (unsigned char) commit->changes[i].file_name[j];
        id = ((id * (byte % 37)) % 15485863) + 1;
      }
    }
  }

  commit->id = malloc(sizeof(char)*10);
  sprintf(commit->id, "%06x", id);
}




// mallocs the nessesary fields in commit
// and sets some of the values in the commit struct
commit* init_commit(helper* h, char* message)
{
  commit* commit = malloc(sizeof(struct commit));
  commit->message = strdup(message);
  commit->n_tracked = h->n_staged;
  commit->tracked = malloc(sizeof(struct file)*(commit->n_tracked));

  // creating deep copy of tracked file names
  for (size_t i = 0; i < commit->n_tracked; i++)
  {
    commit->tracked[i].file_name = strdup(h->staged[i]);
    commit->tracked[i].hash = hash_file(h, h->staged[i]);

    if (NULL == h->active_branch->head)
    {
      // commit->tracked[i].prev_hash = NULL;
      continue;
    }
    // setting the previous hash
    for (size_t j = 0; j < h->active_branch->head->n_tracked; j++)
    {


      if (strcmp(h->active_branch->head->tracked[j].file_name,
                  commit->tracked[i].file_name) == 0)
      {
        commit->tracked[i].prev_hash = h->active_branch->head->tracked[j].hash;
      }
    }
  }

  // first commit in svc
  if (NULL == h->active_branch->head && (strcmp(h->active_branch->name, "master") == 0))
  {
    commit->parent = NULL;
    commit->parent2 = NULL;
    commit->branch = strdup("master");
  }
  else
  {
    commit->parent = h->active_branch->head;
    commit->parent2 = NULL;
    commit->branch = strdup(h->active_branch->name);
  }

  set_changes(h->active_branch->head, commit);
  set_commit_id(commit);

  return commit;
}


// duplicates the folder structure of each file_name, by creating the relevant
// folders
void create_folders(commit* commit)
{
  char command[MAX_COMMAND_SIZE];
  char dest[MAX_DEST_SIZE];

  // creating a new directory, where all the commit's files will be kept
  sprintf(dest, "commits/%s", commit->id);
  sprintf(command, "mkdir %s", dest);
  system(command);

  // each pointer stores the name of each folder in the file_path
  char* splits[25];
  size_t n_splits = 0;

  char* file_path;
  char* temp;

  // creating a similar file structure to the cwd
  for (size_t i = 0; i < commit->n_tracked; i++)
  {
    n_splits = 0;

    // stores the 'folders' section of the file name
    char folders[70] = "";

    // used to check if a folder already exists
    char folder_check[200] = "";


    file_path = strdup(commit->tracked[i].file_name);
    temp = file_path;

    // retrieving all the folders in file_name
    while ((splits[n_splits] = strsep(&file_path, "/")) != NULL)
    {
      n_splits++;
    }

    // if a folder exists in file path - we must create these folders before
    // copying files into them
    if (n_splits > 1)
    {
      for (size_t j = 0; j < n_splits - 1; j++)
      {
        strcat(folders, "/");
        strcat(folders, splits[j]);
      }
    }


    // checking if the folder already exists
    sprintf(folder_check, "'%s/%s'", dest, folders );
    DIR* dir = opendir(folder_check);
    // case whendirectory doesnt not already exists
    if (dir == NULL)
    {
      sprintf(command, "mkdir -p '%s/%s'", dest, folders);
      system(command);
    }

    closedir(dir);
    free(temp);
  }
}

// creates a backup of files tracked by commit
void create_backup(commit* commit)
{
  char command[MAX_COMMAND_SIZE];
  char dest[MAX_DEST_SIZE];
  sprintf(dest, "commits/%s", commit->id);

  create_folders(commit);

  // copies all tracked files into the newly construct folders
  for (size_t i = 0; i < commit->n_tracked; i++)
  {
    sprintf(command, "cp '%s' '%s/%s'", commit->tracked[i].file_name,
                                    dest, commit->tracked[i].file_name);
    system(command);
  }
}

// adds commit to list of commits in helper struct
void add_commit(helper* h, commit* commit)
{
  h->all_commits  = realloc(h->all_commits,
                            sizeof(struct commit*)*(h->n_commits+1));
  h->all_commits[h->n_commits] = commit;
  (h->n_commits)++;
}


// updates the branch heads in teh svc
void update_branches(helper* h, commit* commit)
{
  h->active_branch->head = commit;

  // updating the branch head
  for (size_t i = 0; i < h->n_branches; i++)
  {
    if (strcmp(h->active_branch->head->branch, h->branches[i].name) == 0)
    {
      h->branches[i].head = h->active_branch->head;
    }
  }
}

// returns 1 if there are uncommited changes, else 0
int has_uncommited_changes(helper* h)
{
  // means that there are already files which are
  // added/removed which are not committed
  if (h->has_uncommited_changes == 1)
  {
    return 1;
  }

  if (NULL == h->active_branch->head)
  {
    return 0;
  }

  // checking if file contents have been modified
  for (size_t i = 0; i < h->n_staged; i++)
  {
    for (size_t j = 0; j < h->active_branch->head->n_tracked; j++)
    {
      if (strcmp(h->staged[i], h->active_branch->head->tracked[j].file_name) == 0)
      {
        if (hash_file(h, h->staged[i]) != h->active_branch->head->tracked[j].hash)
        {

          return 1;
        }
      }
    }
  }

  return 0;

}

// restores all files from a commit
void restore_files(commit* commit)
{
  char command[MAX_COMMAND_SIZE];
  sprintf(command, "cp -r commits/%s/* .", commit->id);
  system(command);
}

// very similar to svc_commit except the staged files arent updated based on
// on the cwd and sets a second parent for the commit
char* merge_commit(void *helper, char *message, commit* other_parent) {
    if (NULL == message)
    {
      return NULL;
    }

    struct helper* h = helper;

    if (h->n_staged == 0)
    {

      return NULL;
    }

    if (staged_has_changes(h))
    {
      commit* commit = init_commit(h, message);
      commit->parent2 = other_parent;

      create_backup(commit);

      add_commit(h, commit);

      update_branches(h, commit);

      h->has_uncommited_changes = 0;

      return commit->id;
    }

    return NULL;
}



// copies all relevant resolved files and updates the staged files in the svc
void merge_update_staged(helper* h, commit* main_commit, commit* other_commit,
                        resolution* resolutions, int* n_resolutions)
  {
    // mallocing enough space for the upperbound on the number of
     // files to be staged
     int count = *n_resolutions + main_commit->n_tracked +
                                  other_commit->n_tracked;

     h->staged = malloc(sizeof(char*)*count);
     h->n_staged = 0;


    // updating all the staged files
    for (int i = 0; i < *n_resolutions; i++)
    {
      if (resolutions[i].resolved_file != NULL)
      {
        h->staged[h->n_staged] = strdup(resolutions[i].file_name);
        (h->n_staged)++;

        // copying resolved_files into current directory
        char command[MAX_COMMAND_SIZE];
        sprintf(command, "cp '%s' '%s'", resolutions[i].resolved_file,
                                          resolutions[i].file_name );
        system(command);
      }
    }


    int skip_file = 0;
    // adding tracking files from main commit, , excluding file in resolutions
    for (size_t i = 0; i < main_commit->n_tracked; i++)
    {
      skip_file = 0;

      for (int j = 0; j < *n_resolutions; j++)
      {
        // skipping file of the file is in the resolutions
        if (strcmp(resolutions[j].file_name,
                    main_commit->tracked[i].file_name) == 0)
        {
          skip_file = 1;
          break;
        }
      }

      if (!skip_file)
      {
        h->staged[h->n_staged] = strdup(main_commit->tracked[i].file_name);

        (h->n_staged)++;
      }
    }


    // adding tracking files from other commit, excluding file in resolutions
    for (size_t i = 0; i < other_commit->n_tracked; i++)
    {
      skip_file = 0;

      for (int j = 0; j < *n_resolutions; j++)
      {

        if (strcmp(resolutions[j].file_name,
                    other_commit->tracked[i].file_name) == 0)
        {
          skip_file = 1;
          break;
        }
      }

      // checking if an identical file exists already exists in teh staged
      for (size_t j = 0; j < main_commit->n_tracked; j++)
      {
        if (other_commit->tracked[i].hash == main_commit->tracked[j].hash)
        {
          skip_file = 1;
          break;
        }
      }

      if (!skip_file)
      {
        h->staged[h->n_staged] = strdup(other_commit->tracked[i].file_name);

        (h->n_staged)++;
      }
    }
}
