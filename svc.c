#include "svc.h"
#include "extra_methods.h"


void* svc_init(void) {
  helper* helper = malloc(sizeof(struct helper));

  helper->n_staged = 0;
  helper->staged = malloc(sizeof(char*));

  helper->branches = malloc(sizeof(struct branch));
  helper->branches[0].name = strdup("master");
  helper->branches[0].head = NULL;
  helper->active_branch = &(helper->branches[0]);
  helper->n_branches = 1;

  helper->all_commits = malloc(sizeof(commit*));
  helper->n_commits = 0;

  mkdir("commits", ACCESSPERMS);

  return helper;

}



void cleanup(void* helper) {

  struct helper* h = helper;

  free_staged(h);

  // case when no commits made in the svc
  if (NULL == h->active_branch->head &&
      strcmp(h->active_branch->name, "master") == 0)
  {
    free_all_commits(h);
    free_branches(h);
    free(h);
    return;
  }

  free_all_commits(h);
  free_branches(h);
  free(h);
}




int hash_file(void* helper, char* file_path) {
    if (NULL == file_path)
    {

      return -1;
    }
    else
    {
      FILE* file = fopen(file_path, "r");

      if (NULL == file)
      {

          return -2;
      }

      fseek(file, 0, SEEK_END);
      long int nbytes = ftell(file);
      fseek(file, 0, SEEK_SET);

      int hash = 0;
      unsigned char byte;


      for (size_t i = 0; i < strlen(file_path); i++)
      {
        byte = (unsigned char) file_path[i];
        hash = (hash + byte) % 1000;
      }

      for (int i = 0; i < nbytes; ++i)
      {
        byte = (unsigned char) fgetc(file);
        hash = (hash + byte) % 2000000000;
      }

      return hash;
    }
}



// updates the files in staging area, by checking if they still exist in cwd
void update_staged(helper* h)
{
  char** new_staged = malloc(sizeof(char*)*(h->n_staged));
  size_t n_new_staged = 0;
  FILE* file;


  for (size_t i = 0; i < h->n_staged; i++)
  {
    // attempting to open file to determine if the file exists
    file = fopen(h->staged[i], "r");

    if (file != NULL)
    {
      new_staged[n_new_staged] = strdup(h->staged[i]);
      n_new_staged++;
    }
  }

  free_staged(h);

  h->n_staged = n_new_staged;
  h->staged = new_staged;
}





char* svc_commit(void* helper, char* message) {
    if (NULL == message)
    {
      return NULL;
    }

    struct helper* h = helper;

    // nothing to stage
    if (h->n_staged == 0)
    {

      return NULL;
    }


    update_staged(h);

    // going through through staged files and checking if there have been
    // any changes.
    if (staged_has_changes(h))
    {
      commit* commit = init_commit(h, message);

      create_backup(commit);

      add_commit(h, commit);

      update_branches(h, commit);

      h->has_uncommited_changes = 0;

      return commit->id;
    }

    return NULL;
}



void* get_commit(void* helper, char* commit_id) {
  if (NULL == commit_id)
  {
    return NULL;
  }

  struct helper* h = (struct helper*) helper;

// searches for commit in all commits list
  for (size_t i = 0; i < h->n_commits; i++)
  {
    if (strcmp(h->all_commits[i]->id, commit_id) == 0)
    {
      return h->all_commits[i];
    }
  }

    return NULL;
}


char** get_prev_commits(void* helper, void* commit, int* n_prev) {
    if (NULL == n_prev)
    {
      return NULL;
    }

    struct commit* c = commit;

    // commit null or first commit case
    if (NULL == c || NULL == c->parent)
    {
      *n_prev = 0;

      return NULL;
    }

    // only one prev commit case
    if (NULL == c->parent2)
    {
      char** prev_commits = malloc(sizeof(char*));
      *n_prev = 1;
      prev_commits[0] = c->parent->id;

      return prev_commits;
    }

    // 2 parent commits case
    else
    {
      char** prev_commits = malloc(sizeof(char*)*2);
      *n_prev = 2;

      prev_commits[0] = c->parent->id;
      prev_commits[1] = c->parent2->id;

      return prev_commits;
    }
}


void print_commit(void* helper, char* commit_id) {
  struct helper* h = (struct helper*) helper;

  if (NULL == commit_id)
  {
    printf("Invalid commit id\n");
    return;
  }
  else if (NULL == h->active_branch->head)
  {
    printf("Invalid commit id\n");
    return;
  }

  commit* commit = get_commit(helper, commit_id);

  if (NULL == commit)
  {
    printf("Invalid commit id\n");
    return;
  }


  printf("%s [%s]: %s\n", commit->id, commit->branch, commit->message);

  for (size_t i = 0; i < commit->n_changes; i++)
  {
    if (commit->changes[i].type == ADDED)
    {

      printf("    + %s\n", commit->changes[i].file_name);
    }
    else if (commit->changes[i].type == REMOVED)
    {

      printf("    - %s\n", commit->changes[i].file_name);
    }
    else if (commit->changes[i].type == MODIFIED)
    {
      int hash;
      int prev_hash;

      for (size_t j = 0; j < commit->n_tracked; j++)
      {
        if (strcmp(commit->tracked[j].file_name,
                  commit->changes[i].file_name) == 0)
        {
          hash = commit->tracked[j].hash;
          prev_hash = commit->tracked[j].prev_hash;
        }
      }

      printf("    / %s [%10d -> %10d]\n", commit->changes[i].file_name,
                              prev_hash, hash);
    }
  }

  printf("\n");

  printf("    Tracked files (%lu):\n", commit->n_tracked);

  // printing tracked files
  for (size_t i = 0; i < commit->n_tracked; i++)
  {
    // printf("here\n")
    printf("    [%10d] %s\n", commit->tracked[i].hash,
                            commit->tracked[i].file_name);
  }
}



int svc_branch(void* helper, char* branch_name) {
  if (NULL == branch_name)
  {
    return -1;
  }

  struct helper* h = helper;

  // checking of the branch name is valid
  for (size_t i = 0; i < strlen(branch_name); i++)
  {
    if (!(isalnum(branch_name[i])))
    {
      if (branch_name[i] != '-' && branch_name[i] != '/' &&
          branch_name[i] != '_')
      {
        return -1;
      }
    }
  }

  // checking if the branch name already exists
  for (size_t i = 0; i < h->n_branches; i++)
  {
    if (strcmp(h->branches[i].name, branch_name) == 0)
    {
      return -2;
    }
  }


  if (has_uncommited_changes(h))
  {
    return -3;
  }

  char* active_branch_name = strdup(h->active_branch->name);

  h->branches = realloc(h->branches, sizeof(struct branch)*(h->n_branches+1));
  h->branches[h->n_branches].name = strdup(branch_name);

  // updates active branch - incase pointer addresses get reset during the
   // realloc of h->branches
  for (size_t i = 0; i < h->n_branches; i++)
  {
    if (strcmp(h->branches[i].name, active_branch_name) == 0)
    {
      h->active_branch = &(h->branches[i]);
    }
  }

  h->branches[h->n_branches].head = h->active_branch->head;
  (h->n_branches)++;
  free(active_branch_name);

  return 0;
}


int svc_checkout(void* helper, char* branch_name) {

  if (NULL == branch_name)
  {
    return -1;
  }

  struct helper* h = helper;

  commit* prev_branch_head;

  for (size_t i = 0; i < h->n_branches; i++)
  {
    if (strcmp(h->branches[i].name, branch_name) == 0)
    {
      if (has_uncommited_changes(h))
      {

        return -2;
      }

      prev_branch_head = h->active_branch->head;

      h->active_branch = &(h->branches[i]);

      //case when you check out a branch with no commits -
      if (NULL == h->active_branch->head)
      {
        h->active_branch->head = prev_branch_head;
      }

      // reseting files to the head of the other branch
      restore_files(h->active_branch->head);

      free_staged(h);

      // reassigning all the staged files to the ones in the commit
      h->n_staged = h->active_branch->head->n_tracked;
      h->staged = malloc(sizeof(char*)*h->n_staged);

      for (size_t i = 0; i < h->n_staged; i++)
      {
        h->staged[i] = strdup(h->active_branch->head->tracked[i].file_name);
      }

      return 0;
    }
  }
  return -1;

}


char** list_branches(void* helper, int* n_branches) {
    if (NULL == n_branches)
    {
      return NULL;
    }

    struct helper* h = helper;
    *n_branches = 0;
    char** branch_names = malloc(sizeof(char*)*(h->n_branches));

    for (size_t i = 0; i < h->n_branches; i++)
    {
      branch_names[i] = h->branches[i].name;
      printf("%s\n", branch_names[i]);
      (*n_branches)++;
    }

    return branch_names;
}



int svc_add(void* helper, char* file_name) {
    struct helper* h = helper;

    if (NULL == file_name)
    {
      return -1;
    }
    else
    {
      for (size_t i = 0; i < h->n_staged; i++)
      {
        if (strcmp(file_name, h->staged[i]) == 0)
        {
          return -2;
        }
      }

      FILE* file = fopen(file_name, "r");

      if (NULL == file)
      {
        return -3;
      }

      h->staged = realloc(h->staged, sizeof(char*)*(h->n_staged+1));
      h->staged[h->n_staged] = strdup(file_name);
      (h->n_staged)++;

      h->has_uncommited_changes = 1;

      return hash_file(h, file_name);
    }
}



int svc_rm(void* helper, char* file_name) {
    struct helper* h = helper;

    if (NULL == file_name)
    {
      return -1;
    }
    else
    {
      int found = 0;
      int index;
      int hash;

      // removing and freeing filename from staged list
      for (size_t i = 0; i < h->n_staged; i++)
      {
        if (strcmp(h->staged[i], file_name) == 0)
        {
          found = 1;
          index = i;
          hash = hash_file(h, h->staged[i]);
          free(h->staged[i]);

          break;
        }
      }

      if (!found)
      {
        return -2;
      }

      // shifting file names to fill in the gap in the staged list
      for (size_t i = index; i < h->n_staged - 1; i++)
      {
        h->staged[i] = h->staged[i+1];
      }

      (h->n_staged)--;
      h->has_uncommited_changes = 1;

      return hash;
    }
}



int svc_reset(void* helper, char* commit_id) {

    if (NULL == commit_id)
    {
      return -1;
    }

    struct helper* h = helper;

    if (NULL == h->active_branch->head)
    {
      return -2;
    }

    commit* commit = h->active_branch->head;
    int found = 0;

    // traversing the commits list backwards until the commit is found or the
    // first commit is reached
    while (commit != NULL)
    {
      if (strcmp(commit->id, commit_id) == 0)
      {
        found = 1;
        break;
      }

      commit = commit->parent;
    }


    if (found)
    {
      restore_files(commit);
      free_staged(h);

      // reassigning the staged files from the new commit
      h->n_staged = commit->n_tracked;
      h->staged = malloc(sizeof(char*)*h->n_staged);

      for (size_t i = 0; i < h->n_staged; i++)
      {
        h->staged[i] = strdup(commit->tracked[i].file_name);
      }

      // moving the active branch pointer
      for (size_t i = 0; i < h->n_branches; i++)
      {
        if (strcmp(h->active_branch->name, h->branches[i].name) == 0)
        {
          h->branches[i].head = commit;
          h->active_branch = &(h->branches[i]);
        }
      }

      return 0;
    }
    else
    {
      return -2;
    }
}



char* svc_merge(void* helper, char* branch_name, struct resolution* resolutions,
                    int n_resolutions) {

    if (NULL ==  branch_name)
    {
      printf("Invalid branch name\n");
      return NULL;
    }

    struct helper* h = helper;
    commit* main_commit = h->active_branch->head;
    commit* other_commit;

    if (strcmp(h->active_branch->name, branch_name) == 0)
    {
      printf("Cannot merge a branch with itself\n");
      return NULL;
    }

    int found = 0;
    for (size_t i = 0; i < h->n_branches; i++)
    {
      if (strcmp(h->branches[i].name, branch_name) == 0)
      {
        found = 1;
        other_commit = h->branches[i].head;
        break;
      }
    }

    if (!found)
    {
      printf("Branch not found\n");
      return NULL;
    }

    if (has_uncommited_changes(h))
    {
      printf("Changes must be committed\n");
      return NULL;
    }

    restore_files(other_commit);
    free_staged(h);

    merge_update_staged(h, main_commit, other_commit, resolutions,
                                                      &n_resolutions);


    // creating a new commit
    char* message = malloc(sizeof(char)*50);
    sprintf(message, "Merged branch %s", other_commit->branch);
    char* commit_id = merge_commit(h, &message[0], other_commit);
    free(message);

    printf("Merge successful\n");

    return commit_id;
}
