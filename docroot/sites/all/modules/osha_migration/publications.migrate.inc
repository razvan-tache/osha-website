<?php
/**
 * @file
 * Publication Content Type Migration
 * PHP version 5
 *
 * @category Migration
 * @package  OSHA
 * @author   Razvan Tache <razvan.tache@eaudeweb.ro>
 * @license  http://www.gnu.org/copyleft/gpl.html GNU General Public License
 * @link     none
 */

/**
 * Class OshaMigratePublications
 *
 * @category Migration
 * @package  OSHA
 * @author   Razvan Tache <razvan.tache@eaudeweb.ro>
 * @license  http://www.gnu.org/copyleft/gpl.html GNU General Public License
 * @link     none
 */

class OshaMigratePublications extends DynamicMigration {

  protected $contentType = NULL;

  /**
   * Mapping of the source to destination.
   *
   * @param array $arguments
   *   list of arguments provided by the migration module
   */
  public function __construct($arguments = array()) {
    parent::__construct();

    $this->description = 'Import "Publications" taxonomy from CSV file';
    $this->contentType = 'publication';
    $this->dependencies = array('PublicationsFiles');

    $file_name = NULL;
    if (isset($arguments['file_name'])) {
      $file_name = $arguments['file_name'];
    }

    $multilingual_fields = array('title', 'description', 'order_id', 'file');
    $source_file = sprintf('%s/%s', drupal_get_path('module', 'osha_migration'), $file_name);
    $this->source = new MigrateSourceList(new PublicationMigrateListCSV($source_file, $multilingual_fields),
      new PublicationMigrateItemCSV(), $this->fields());

    $this->map = new MigrateSQLMap($this->machineName,
      array(
        'id' => array(
          'type' => 'varchar',
          'length' => 200,
          'not null' => TRUE,
          'description' => 'Nace code',
        ),
      ),
      MigrateDestinationNode::getKeySchema()
    );

    $node_options = MigrateDestinationNode::options('en', 'text');
    $this->destination = new MigrateDestinationNode($this->contentType, $node_options);

    $this->addFieldMapping('body:format')->defaultValue('plain_text');
    $this->addFieldMapping('field_tags', 'subjectTids')->separator(',');
    $this->addFieldMapping('field_tags:source_type')->defaultValue('tid');
    $this->addFieldMapping('field_thesaurus', 'thesaurusTids')->separator(',');
    $this->addFieldMapping('field_thesaurus:source_type')->defaultValue('tid');
    $this->addFieldMapping('field_nace_codes', 'naceTids')->separator(',');
    $this->addFieldMapping('field_nace_codes:source_type')->defaultValue('tid');
    $this->addFieldMapping('field_publication_type', 'pubTypeTid')
      ->separator(',');
    $this->addFieldMapping('field_publication_type:source_type')
      ->defaultValue('tid');
    $this->addFieldMapping('field_publication_author', 'author');
    $this->addFieldMapping('field_publication_date', 'publication_date');
    $this->addFieldMapping('created', 'creation_date');
    $this->addFieldMapping('changed', 'modification_date');
    $this->addFieldMapping('uid')->defaultValue(1);
    $this->addFieldMapping('field_publication_file:preserve_files')->defaultValue(TRUE);
  }

  /**
   * Generates the migration machine name.
   *
   * @param null $class_name
   *   By default, the migration machine name is the class name
   *  (with the Migration suffix, if present, stripped)
   *
   * @return string
   *   Migration name
   */
  protected function generateMachineName($class_name = NULL) {
    return 'PublicationsContentType';
  }

  /**
   * Prepares the source row before migration.
   *
   * @param object $row
   *   One source $row as returned by getItem
   *
   * @return bool
   *   TRUE if row is valid
   */
  public function prepareRow($row) {
    $row->language = 'en';

    $tags = explode(', ', mapCategories($row->subject));
    $subcat = explode(', ', mapCategories($row->subcategory));
    $tags = array_unique(array_merge($tags, $subcat));
    $row->subjectTids = $this->getTidsFromField('tags', 'field_tags_code', implode(', ', $tags));
    $row->thesaurusTids = $this->getTidsFromField('thesaurus', 'field_thesaurus_code', $row->multilingual_thesaurus);
    $row->naceTids = $this->getTidsFromField('nace_codes', 'field_nace_code', $row->nace);
    $row->pubTypeTid = $this->getTidsFromField('publication_types', 'field_publication_type_code', $row->publication_type);

    return TRUE;
  }

  /**
   * Returns Tids from the vocabulary $voc, matching $data on $field.
   *
   * @param string $voc
   *   The vocabulary within the search has place
   * @param string $field
   *   The field to match
   * @param array $data
   *   An array of data to match against $field
   *
   * @return null|string
   *   A list of Tids as string, separated by ',' or null if no corresponding
   *   results are found
   */
  private function getTidsFromField($voc, $field, $data) {
    $data_array = explode(', ', $data);
    $results = getTids($voc, $field, $data_array);
    if (!empty($results)) {
      return implode(', ', $results);
    }
    return NULL;
  }

  /**
   * Prepares the destination entity before migration.
   *
   * @param object $entity
   *   The $entity to be modified
   * @param object $row
   *   The $row with the source data
   */
  public function prepare($entity, $row) {
    $entity->language = 'en';
    $translations_data = array();

    foreach ($row->title as $language => $translation) {
      if (!empty($row->title->$language)) {
        $entity->title_field[$language][0]['value'] = $translation;
      }
      $source_language = ($language == 'en' ? '' : 'en');
      $translations_data[$language] = array(
        'entity_type' => 'node',
        'language' => $language,
        'source' => $source_language,
        'status' => '1',
        'translate' => '0',
      );
    }

    foreach ($row->description as $language => $translation) {
      if (!empty($row->description->$language)) {
        $entity->body[$language][0]['value'] = $translation;
      }
    }

    foreach ($row->order_id as $language => $translation) {
      if (!empty($language)) {
        $entity->field_publication_bookshop_id[$language][0]['value'] = $translation;
      }
    }

    foreach ($row->file as $language => $translation) {
      if (!empty($language)) {
        $uri = "public://publications/documents" . $translation;
        $file_array = file_load_multiple($fids = array(), $conditions = array('uri' => $uri));
        $file = current($file_array);
        $entity->field_publication_file[$language][0]['fid'] = $file->fid;
        $entity->field_publication_file[$language][0]['display'] = 1;
      }
    }

    $entity->translations = (object) array(
      'original' => 'en',
      'data' => $translations_data,
    );
  }

  /**
   * Prepares the $entity object for insertion.
   *
   * This function sets the node aliases after the migration,
   * when node nid is available.
   *
   * The same alias will be used for all languages (see mappings).
   *
   * @param object $entity
   *   The $entity to be modified
   * @param object $row
   *   The $row with the source data
   */
  public function complete($entity, $row) {
    $languages = array_keys($entity->body);
    foreach ($languages as $lang_code) {
      $args = array(
        'source' => 'node/' . $entity->nid,
        'alias' => $row->id,
        'language' => $lang_code,
      );
      path_save($args);
    }
  }

  /**
   * Return an array of mapped fields from source.
   *
   * @return array
   *   The array of fields
   */
  private function fields() {
    return array(
      'id' => "id",
    );
  }
}

/**
 * Class PublicationMigrateListCSV
 */
class PublicationMigrateListCSV extends MigrateList {

  protected $fileUrl = '';
  protected $httpOptions;
  protected $multilingualFields;
  public static $csvData;

  /**
   * Populates csvData with parsed information from $file_url.
   *
   * @param string $file_url
   *   The location of the file with source raw data
   * @param array $multilingual_fields
   *   An array with each field name from source file, that has translations
   * @param array $http_options
   *   Http Options for future tweaks
   */
  public function __construct($file_url, $multilingual_fields = array(), $http_options = array()) {
    parent::__construct();
    $this->fileUrl = $file_url;
    $this->httpOptions = $http_options;
    $this->multilingualFields = $multilingual_fields;

    self::$csvData = $this->parseMultilingualCSV($this->fileUrl, $multilingual_fields);
  }

  /**
   * Returns the file url.
   *
   * @return string
   *   The file url
   */
  public function __toString() {
    return $this->fileUrl;
  }

  /**
   * Parses the csv $file and molds the data, to best suit our needs.
   *
   * The data is cached
   *
   * @param string $file
   *   The csv file url
   * @param array $multi_fields
   *   Multi lingual fields
   *
   * @return array
   *   Returns assoc array of assoc arrays which represents one source row
   *   array[id][field] -- row
   */
  protected function parseMultilingualCSV($file, $multi_fields = array()) {
    $handle = fopen($file, 'r');
    $fields = $data = fgetcsv($handle, 1000, ';');

    $final = [];

    while (($data = fgetcsv($handle, 100000, ';')) !== FALSE) {
      $country = explode('/', $data[0], 3)[1];
      $id = explode('/', $data[0], 3)[2];
      $publication_type = explode('/', $data[0])[3];

      foreach ($fields as $key => $field) {
        if (in_array($field, $multi_fields)) {
          $final[$id][$field][$country] = $data[$key];
          continue;
        }
        $final[$id][$field] = $data[$key];
      }

      $final[$id]['id'] = $id;
      $final[$id]['publication_type'] = $publication_type;
    }

    foreach ($final as $id => $id_value) {
      foreach ($id_value as $key_field => $field) {
        if (is_array($field) && !isset($final[$id][$key_field]['en'])) {
          unset($final[$id]);
        }
      }

      if ($final[$id]['workflow_state'] !== 'published') {
        unset($final[$id]);
      }
    }
    return $final;
  }

  /**
   * Gets an array of ids from cache.
   *
   * @return array|Mixed|null
   *   returns array of ids
   */
  public function getIdList() {
    if (self::$csvData) {
      $ids = array();
      foreach (self::$csvData as $key => $value) {
        $ids[] = $key;
      }
      return $ids;
    }
    Migration::displayMessage(t('Loading of !fileUrl failed:',
      array('!listurl' => $this->fileUrl)));
    return NULL;
  }

  /**
   * Counts the number of source rows.
   *
   * @return int
   *   Number of rows to be migrated from source
   */
  public function computeCount() {
    return count(self::$csvData);
  }
}

/**
 * Class PublicationMigrateItemCSV
 */
class PublicationMigrateItemCSV extends MigrateItem {

  protected $data;

  /**
   * Gets a source row by id - uses caching system.
   *
   * @param mixed $id
   *   The id to search by
   *
   * @return null|stdClass
   *   The corresponding source row or NULL if not found
   */
  public function getItem($id) {
    // Object caching.
    if (empty($this->data)) {
      $csv_data = PublicationMigrateListCSV::$csvData;
      foreach ($csv_data as $csv_id => $item) {
        $this->data[$csv_id] = json_decode(json_encode($item), FALSE);
      }
    }

    return isset($this->data[$id]) ? $this->data[$id] : NULL;
  }
}

/**
 * Class MigrateOshaPublicationFiles
 */
class OshaMigratePublicationsFiles extends DynamicMigration {

  /**
   * Mapping of the source to destination.
   *
   * @param array $arguments
   *   list of arguments provided by the migration module
   */
  public function __construct($arguments) {
    parent::__construct();

    $this->description = t('Imports files needed bt publication');
    $file_name = NULL;
    if (isset($arguments['file_name'])) {
      $file_name = $arguments['file_name'];
    }

    $source_file = sprintf('%s/%s', drupal_get_path('module', 'osha_migration'), $file_name);
    $this->source = new MigrateSourceList(new PublicationMigrateFilesListCSV($source_file),
      new PublicationMigrateFilesItemCSV(), $this->fields());

    $this->map = new MigrateSQLMap($this->machineName,
      array(
        'id' => array(
          'type' => 'varchar',
          'length' => 200,
          'not null' => TRUE,
          'description' => 'The old path of the file',
        ),
      ),
      MigrateDestinationFile::getKeySchema()
    );

    $this->destination = new MigrateDestinationFile('file');

    $this->addFieldMapping('uid')->defaultValue(1);
    $this->addFieldMapping('source_dir')->defaultValue(osha_migration_get_data_dir() . "/export");
    $this->addFieldMapping('value', 'file');
    $this->addFieldMapping('destination_file', 'file');
    $this->addFieldMapping('destination_dir')->defaultValue('public://publications/documents');
    $this->addFieldMapping('preserve_files')->defaultValue(FALSE);
    $this->addFieldMapping('file_replace')->defaultValue(FILE_EXISTS_REPLACE);
    $this->addFieldMapping('timestamp', 'timestamp');
  }

  /**
   * Generates the migration machine name.
   *
   * @param null $class_name
   *   By default, the migration machine name is the class name
   *  (with the Migration suffix, if present, stripped)
   *
   * @return string
   *   Migration name
   */
  protected function generateMachineName($class_name = NULL) {
    return 'PublicationsFiles';
  }

  /**
   * Prepares the source row before migration.
   *
   * @param object $row
   *   One source $row as returned by getItem
   *
   * @return bool
   *   TRUE if row is valid
   */
  public function prepareRow($row) {
    $row->timestamp = time();

    return TRUE;
  }

  /**
   * Return an array of mapped fields from source.
   *
   * @return array
   *   The array of fields
   */
  private function fields() {
    return array(
      'id' => "It's used for mapping internally, and it's actually the old path to the file, as it is unique",
      'file' => "The old path to the file",
      'timestamp' => "Represents the moment the file got into Drupal",
    );
  }
}

class PublicationMigrateFilesListCSV extends MigrateList {

  protected $fileUrl = '';
  protected $httpOptions;
  protected $multilingualFields;
  public static $csvData;

  /**
   * Populates csvData with parsed information from $file_url.
   *
   * @param string $file_url
   *   The location of the file with source raw data
   * @param array $http_options
   *   Http Options for future tweaks
   */
  public function __construct($file_url, $http_options = array()) {
    parent::__construct();
    $this->fileUrl = $file_url;
    $this->httpOptions = $http_options;

    self::$csvData = $this->parseMultilingualCSV($this->fileUrl);
  }

  /**
   * Returns the file url.
   *
   * @return string
   *   The file url
   */
  public function __toString() {
    return $this->fileUrl;
  }

  /**
   * Parses the csv $file and molds the data, to best suit our needs.
   *
   * The data is cached
   *
   * @param string $file
   *   The csv file url
   *
   * @return array
   *   Returns assoc array of assoc arrays which represents one source row
   *   array[id][field] -- row
   */
  protected function parseMultilingualCSV($file) {
    $handle = fopen($file, 'r');
    $fields = $data = fgetcsv($handle, 1000, ';');

    $final = [];

    while (($data = fgetcsv($handle, 100000, ';')) !== FALSE) {
      $id = $data[0];
      $publication_type = explode('/', $data[0])[3];

      foreach ($fields as $key => $field) {
        $final[$id][$field] = $data[$key];
      }

      $final[$id]['id'] = $id;
      $final[$id]['publication_type'] = $publication_type;
    }

    foreach ($final as $id => $id_value) {

      if ($final[$id]['workflow_state'] !== 'published') {
        unset($final[$id]);
      }

      if (strlen($final[$id]['file']) > 254) {
        unset($final[$id]);
      }
    }

    return $final;
  }

  /**
   * Gets an array of ids from cache.
   *
   * @return array|Mixed|null
   *   returns array of ids
   */
  public function getIdList() {
    if (self::$csvData) {
      $ids = array();
      foreach (self::$csvData as $key => $value) {
        $ids[] = $key;
      }
      return $ids;
    }
    Migration::displayMessage(t('Loading of !fileUrl failed:',
      array('!listurl' => $this->fileUrl)));
    return NULL;
  }

  /**
   * Counts the number of source rows.
   *
   * @return int
   *   Number of rows to be migrated from source
   */
  public function computeCount() {
    return count(self::$csvData);
  }
}

/**
 * Class PublicationMigrateFilesItemCSV
 */
class PublicationMigrateFilesItemCSV extends MigrateItem {
  protected $data;

  /**
   * Gets a source row by id - uses caching system.
   *
   * @param mixed $id
   *   The id to search by
   *
   * @return null|stdClass
   *   The corresponding source row or NULL if not found
   */
  public function getItem($id) {

    if (empty($this->data)) {
      $csv_data = PublicationMigrateFilesListCSV::$csvData;
      foreach ($csv_data as $csv_id => $item) {
        $this->data[$csv_id] = json_decode(json_encode($item), FALSE);
      }
    }

    return isset($this->data[$id]) ? $this->data[$id] : NULL;
  }

}

/**
 * Gets a string of old categories and returns a string of new ones.
 *
 * @param string $categories
 *   String with old categories and subcategories separated by ', '
 *
 * @return string
 *   String with the new version of categories
 */
function mapCategories($categories) {
  $categories = explode(', ', $categories);

  $new_categories = array();
  foreach ($categories as $cat) {
    if ($cat == 'young_people::educators_mainstreaming') {
      $new_categories[] = 'mainstreaming_osh_into_education';
    }
    elseif (count(explode('::', $cat)) == 2) {
      $exp = explode('::', $cat);
      $new_categories[] = $exp[0];
    }
    else {
      $new_categories[] = $cat;
    }
  }

  $categories = $new_categories;

  $initial = array(
    'accident_prevention',
    'ageing',
    'businessOSH',
    'campaign_2014_15',
    'dangerous_substances',
    'economic_incentives',
    'education',
    'osh_education',
    'osheducation',
    'esener',
    'foresight',
    'gender',
    'green_jobs',
    'legislation',
    'management_leadership',
    'msd',
    'nanotechnology',
    'prevention',
    'risk_observatory',
    'risks',
    'stress',
    'young_people',
  );

  $replace = array(
    'accident_prevention',
    'ageing_and_osh',
    'good_osh_is_good_for_business',
    'campaign_2014_2015',
    'dangerous_substances',
    'good_osh_is_good_for_business',
    'mainstreaming_osh_into_education',
    'mainstreaming_osh_into_education',
    'mainstreaming_osh_into_education',
    'esener',
    'emerging_risks',
    'women_and_osh',
    'green_jobs',
    'legislation',
    'leadership',
    'musculoskeletal_disorders',
    'nanomaterials',
    'accident_prevention',
    'emerging_risks',
    'emerging_risks',
    'psychosocial_risks_and_stress',
    'osh_and_young_people',
  );

  $removed = array(
    'agriculture',
    'Campaign 2012-13',
    'cleaners',
    'construction',
    'csr',
    'cww',
    'disabled_people',
    'fisheries',
    'health_care',
    'healthcare',
    'horeca',
    'maintenance',
    'migrant_workers',
    'noise',
    'organisation',
    'Practical Solutions, Good Practice',
    'research',
    'risk_assessment',
    'road_transport',
    'statistics',
    'transport',
    'whp',
    'worker_participation',
    'competitions',
    'disability',
    'hwi',
    'oira',
    'presidency',
  );

  $replace = array_combine($initial, $replace);
  $new_categories = array();

  $categories = array_diff($categories, $removed);

  foreach ($categories as $cat) {
    $new_categories[] = $replace[$cat];
  }

  return implode(', ', $new_categories);
}

/**
 * Returns corresponding tids from $vocabulary.$field_match on $term_codes.
 *
 * @param string $vocabulary
 *   The vocabulary within the search has place.
 * @param string $field_match
 *   The field the match is made on within in vocabulary
 * @param array $term_codes
 *   The term_codes that should be found in $vocabulary.$field_match
 *
 * @return array
 *   Returns an array of corresponding tids
 */
function getTids($vocabulary, $field_match, $term_codes) {
  static $cache = array();
  $ret = array();
  if (empty($cache[$vocabulary][$field_match])) {
    $voc = taxonomy_vocabulary_machine_name_load($vocabulary);
    $terms = taxonomy_get_tree($voc->vid, 0, NULL, TRUE);
    foreach ($terms as $term) {
      if (!empty($term->$field_match)) {
        $field = $term->$field_match;
        $cache[$vocabulary][$field_match][$field['und'][0]['value']] = $term->tid;
      }
    }
  }

  if (!empty($cache[$vocabulary][$field_match])) {
    foreach ($term_codes as $t) {
      if (!empty($cache[$vocabulary][$field_match][$t])) {
        $ret[] = $cache[$vocabulary][$field_match][$t];
      }
    }
  }

  return $ret;
}
